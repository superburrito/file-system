#include "assignment3_helpers.h"

my_file_system* create_new_file_system(unsigned int s){
	printf("[INIT] File system created with partitions of size: %d\n", s);
	my_file_system* ret = (my_file_system *) malloc(sizeof(my_file_system));
	ret->size = s;
	ret->first_partition = calloc(s, 1);
	ret->second_partition = calloc(s,1);
	return ret;
}

int save_partition(void* ptnPtr, int ptnSize, char* fileName){
	FILE* filePtr = fopen(fileName, "w");
	int fileFd = fileno(filePtr);
	int bytesWritten = write(fileFd, ptnPtr, ptnSize);
	free(ptnPtr);
	printf("[SAVE] %d bytes written to external file %s.\n", bytesWritten, fileName);
	close(fileFd);
	return bytesWritten;
}

void* load_partition(int ptnSize, char* fileName){
	void* newPtnPtr = calloc(ptnSize, 1);
	FILE* filePtr = fopen(fileName, "r");
	int fileFd = fileno(filePtr);
	int bytesRead = read(fileFd, newPtnPtr, ptnSize);
	printf("[LOAD] %d bytes read from external file %s.\n", bytesRead, fileName);
	close(fileFd);
	return newPtnPtr;
}

int create_file(char* fileName, int fileSize, void* ptnPtr, int ptnSize, int rwef){
	printf("[CREATE] Attempting to create %s...\n", fileName);

	// TEST: Check if fileName is unique (i.e. not used currently)
	if(count_matching_md(ptnPtr, fileName) > 0){
		printf("[CREATE] WARNING -- %s already exists in partition.\n", fileName);
		return 1;
	}

	// 1. Based on filepath, deduce if file should have parents
	md* immedParentMdPtr;
	int numOfParents = count_parents(fileName);
	if(numOfParents > 0){
		// Check if parents *actually* exist
		check_parents(fileName, ptnPtr, numOfParents);
		// Get parent's path name
		char parentNameBuffer[strlen(fileName)];
		get_parent_path(fileName, parentNameBuffer, numOfParents);
		immedParentMdPtr = find_matching_md(ptnPtr, parentNameBuffer);
	} else {
		immedParentMdPtr = NULL;
	}

	// TEST: Check if there is enough space on the ptn 
	//       (for both the file AND the associated spare space)
	int bytesFree = calculate_free_bytes(ptnPtr, ptnSize);
	if(bytesFree < fileSize + (fileSize/SPARE_SIZE_PROPORTION)){
		printf("[CREATE] WARNING -- Insufficient space for %s.", fileName);
		return 2;
	}

	// Initialise offset that marks the first byte of the new file
	int newFirstByteOffset;

	// 2. Count number of valid md blocks in md table
	int numOfValidFiles = count_valid_md_blocks(ptnPtr);

	// If there are no valid md blocks, ptn is "empty"
	// First byte starts at the end of md table 
	if (numOfValidFiles == 0){
		newFirstByteOffset = MD_TABLE_SIZE;
		printf("[CREATE] As no files exist, %s is automatically positioned at the front.\n", fileName);
	// If there are valid blocks...
	} else {
		// 3. Create an array of sorted md_snaps. This is the partition metadata.
		md_snap* mdSnapsPtr = create_sorted_md_snaps(ptnPtr, numOfValidFiles);

		// 4. Traverse through mdSnaps array to find the appropriate 
		//	  place the first byte of the new file.
		// 4.1. First, check if there is space before the first file (include spare size);
		int requiredSize = fileSize + (fileSize/SPARE_SIZE_PROPORTION);
		if(mdSnapsPtr[0].firstByteOffset - MD_TABLE_SIZE > requiredSize){
			newFirstByteOffset = 0; 
			printf("[CREATE] %s positioned before the first file in this partition.\n", fileName);
		// 4.2. Then, check if there is space after the last file
		} else if (ptnSize - mdSnapsPtr[numOfValidFiles-1].lastByteOffset > requiredSize){
			newFirstByteOffset = mdSnapsPtr[numOfValidFiles-1].lastByteOffset; 
			printf("[CREATE] %s positioned after the last file in this partition.\n", fileName);
		// 4.3. Check for suitable spaces in the intervals between files
		} else {
			int intervalFound = 0;
			for(int i=1; i<numOfValidFiles; i++){
				int interval = mdSnapsPtr[i].firstByteOffset - mdSnapsPtr[i-1].lastByteOffset;
				if(interval > requiredSize){
					intervalFound++;
					newFirstByteOffset = mdSnapsPtr[i-1].lastByteOffset;
					printf("[CREATE] %s positioned right after %s.\n", fileName, mdSnapsPtr[i-1].name);
					break;
				}
			}
			// If no suitable intervals are found... TIME 2 DEFRAGMENT  
			if (intervalFound == 0){
				free(mdSnapsPtr);
				defragment(ptnPtr);
				return create_file(fileName, fileSize, ptnPtr, ptnSize, rwef);
			}
		}
		// 5. We now know where to place the first byte, so we free mdSnaps
		//    as we don't need it anymore.
		free(mdSnapsPtr);
	} 	

	// 6. Find empty md block in the md table + fill it up with data
	md* newMdPtr = find_empty_md_block(ptnPtr);
	md* filledMdPtr = fill_md_block(newMdPtr, fileName, fileSize, newFirstByteOffset, immedParentMdPtr, rwef);

	// 7. New md block has been created, now we can direct the newfile's 
	// 	  parents to point to it -- only if the new file has a parent
	if(numOfParents > 0){
		printf("[CREATE] Including metadata pointer in %s's parent folder...\n", fileName);
		add_child_to_folder(filledMdPtr->parentMdPtr->name, ptnPtr, filledMdPtr);
	}
	return 0;
}


int create_folder(char* folderName, void* ptnPtr, int ptnSize, int rwef){
	return create_file(folderName, MAX_FOLDER_SIZE, ptnPtr, ptnSize, rwef);
}


// Requires a type that can be 'w' or 'r'
file_handler* open_file(char* fileName, void* ptnPtr, char openType){
	printf("[OPEN] Opening %s...\n", fileName);
	ensure_file_exists(ptnPtr, fileName);
	md* matchingMdPtr = find_matching_md(ptnPtr, fileName);
	// Create a new fileHandler and return it
	file_handler* newFileHandlerPtr = malloc(sizeof(file_handler));
	strcpy(newFileHandlerPtr->name, matchingMdPtr->name);
	newFileHandlerPtr->size = matchingMdPtr->size;
	newFileHandlerPtr->firstByteOffset = matchingMdPtr->firstByteOffset;
	newFileHandlerPtr->handlingOffset = matchingMdPtr->firstByteOffset;
	newFileHandlerPtr->type = openType;
	printf("[OPEN] File Handler for %s successfully created.\n", matchingMdPtr->name);
	return newFileHandlerPtr;
}

int close_file(file_handler* fileHandlerPtr){
	printf("[CLOSE] Closing %s...\n", fileHandlerPtr->name);
	free(fileHandlerPtr);
	return 0;
}

int delete_file(char* fileName, void* ptnPtr){
	printf("[DELETE] Deleting %s...\n", fileName);
	ensure_file_exists(ptnPtr, fileName);
	md* mdPtr = find_matching_md(ptnPtr, fileName);
	// If file is a folder
	if(mdPtr->rwef % 10 == 1){
		delete_children_in_folder(fileName, ptnPtr);
	} 
/*	// If file has a parent
	if (mdPtr->parentMdPtr != NULL){
		remove_child_from_folder(mdPtr->parentMdPtr->name, ptnPtr, mdPtr);	
	}*/
	mdPtr->valid = 0;
	printf("[DELETE] %s deleted.\n", fileName);
	return 0;
}

void* read_file(file_handler* fileHandlerPtr, void* buffer, int numBytes, void* ptnPtr){
	printf("[READ] Reading from %s...\n", fileHandlerPtr->name);
	// TEST: First check that file exists
	ensure_file_exists(ptnPtr, fileHandlerPtr->name);

	// TEST: fileHandler should be for reading
	assert(fileHandlerPtr->type = 'r');

	// 1. Get ptr to current reading position 
	char* parserPtr = convert_offset_to_curr_pos_ptr(ptnPtr, fileHandlerPtr);
	
	// 2. Check remaining bytes to EOF
	int remainingBytesToEOF = calculate_bytes_to_EOF(fileHandlerPtr);

	// 3. Use memcpy to read data to buffer +
	//    Update fileHandler based on how many bytes were read
	// 3.1. If we read numBytes 
	if(numBytes <= remainingBytesToEOF){
		memcpy(buffer, parserPtr, numBytes);
		fileHandlerPtr->handlingOffset += numBytes;
		printf("[READ] %d bytes read.\n", numBytes);
	// 3.2. If we only read remainingBytesToEOF
	} else {
		memcpy(buffer, parserPtr, remainingBytesToEOF);
		fileHandlerPtr->handlingOffset += remainingBytesToEOF;
		printf("[READ] %d bytes read.\n", remainingBytesToEOF);
	}

	// Return buffer that holds read data
	return buffer;
}

int write_file(file_handler* fileHandlerPtr, void* buffer, int numBytes, void* ptnPtr, int ptnSize){
	printf("[WRITE] Writing to %s...\n", fileHandlerPtr->name);
	// TEST: First check that file exists
	ensure_file_exists(ptnPtr, fileHandlerPtr->name);
	// TEST: fileHandler should be for reading
	assert(fileHandlerPtr->type = 'w');

	// 1. Cast ptnPtr to char and move it to current writing offset
	char* parserPtr = convert_offset_to_curr_pos_ptr(ptnPtr, fileHandlerPtr);

	// 2. Check remaining bytes to EOF
	int remainingBytesToEOF = calculate_bytes_to_EOF(fileHandlerPtr);

	// 3. Use memcpy to write data to parserPtr +
    //	  Update fileHandler based on how many bytes were written
	// 3.1. If numBytes is less than remainingBytesToEOF,
	//      then we don't have to extend the file. 
	//   	Write nbytes and update fileHandler
	if (numBytes <= remainingBytesToEOF) {
		memcpy(parserPtr, buffer, numBytes);
		fileHandlerPtr->handlingOffset += numBytes;
		printf("[WRITE] %d bytes written.\n", numBytes);
    // 3.2. If numBytes is greater than remainingBytesToEOF, 
    //      we need to extend the file.
	} else {
		// 3.2.1 Calculate new file size
		int newSize = fileHandlerPtr->size + (numBytes - remainingBytesToEOF);
		// 3.2.2 Extend file
		extend_file(fileHandlerPtr->name, newSize, ptnPtr, ptnSize);

		// 3.2.3 When file has been extended, Update this fileHandler with...
		//		 another fileHandler. It is possible that extend caused this file 
		// 		 to move, so we also have to update this fileHandler's offsets.
		file_handler* tempHandlerPtr = open_file(fileHandlerPtr->name, ptnPtr, 'r');
		fileHandlerPtr->size = tempHandlerPtr->size;
		fileHandlerPtr->firstByteOffset = tempHandlerPtr->firstByteOffset;
		// Handling offset is updated relative to the NEW firstByteOffset
		fileHandlerPtr->handlingOffset += (tempHandlerPtr->firstByteOffset - fileHandlerPtr->firstByteOffset);
		// Name has not changed, no need to update
		// Free the temporary handler, since original handler has been updated
		free(tempHandlerPtr);
	}

	// Update modifiedAt time and return
	printf("[WRITE] Updating modifiedAt for %s\n", fileHandlerPtr->name);
	md* mdPtr = find_matching_md(ptnPtr, fileHandlerPtr->name);
	mdPtr->modifiedAt = time(NULL);
	return 0;
}


file_handler* rewind_handler(file_handler* fileHandlerPtr){
	fileHandlerPtr->handlingOffset = fileHandlerPtr->firstByteOffset;
	return fileHandlerPtr;
}


int move_to_partition(char* fileName, void* fromPtnPtr, void* toPtnPtr, int ptnSize){
	printf("[MOVE] Moving %s to another partition...\n", fileName);
	md* fileMdPtr = find_matching_md(fromPtnPtr, fileName);
	
	// If file is a folder
	/*if(fileMdPtr->rwef % 10 ==1){
		move_children_in_folder(fileName, fromPtnPtr, toPtnPtr, ptnSize);
	}*/	

	int recreatedSize = fileMdPtr->size;
	return recreate(fileName, recreatedSize, fromPtnPtr, toPtnPtr, ptnSize);
}

int new_parent(char* fileName, char* fromParentName, char* toParentName, void* ptnPtr){
	printf("[MOVE] Moving %s to another folder...\n", fileName);
	md* fileMdPtr = find_matching_md(ptnPtr, fileName);
	remove_child_from_folder(fromParentName, ptnPtr, fileMdPtr);
	add_child_to_folder(toParentName, ptnPtr, fileMdPtr);
	md* newParentMdPtr = find_matching_md(ptnPtr, toParentName);
	// Update shifted file's own metadata details
	fileMdPtr->parentMdPtr = newParentMdPtr;
	fileMdPtr->modifiedAt = time(NULL);

	// Create buffers that will form the new name
	char newNameBuffer[MAX_FILE_NAME_SIZE];
	memset(newNameBuffer, '\0', MAX_FILE_NAME_SIZE);
	char lastPathSegmentBuffer[MAX_FILE_NAME_SIZE];

	// Add new parent's directory to the new name
	strcat(newNameBuffer, newParentMdPtr->name);
	// Add a '/' to the new name
	strcat(newNameBuffer, "/");
	// Add last path segment of current file (i.e. a.txt) to the new name
	get_last_path_segment(fileName, lastPathSegmentBuffer);
	strcat(newNameBuffer, lastPathSegmentBuffer);
	// Load buffer into metadata block
	strcpy(fileMdPtr->name, newNameBuffer);

	return 0;
}


int extend_file(char* fileName, int newSize, void* ptnPtr, int ptnSize){
	printf("[EXTEND] Attempting to increase file size of %s to %d...\n", fileName, newSize);
	// TEST: This is a file modifier, so first check that file exists
	ensure_file_exists(ptnPtr, fileName);

	// 1. Get md block associted with file to be extended
	md* fileMdPtr = find_matching_md(ptnPtr, fileName);
	// TEST: Clearly, new size should be less than current size 
	assert(newSize > fileMdPtr->size);
	// 2. Calculate the extra size required
	int extraSizeRequired = newSize - fileMdPtr->size;
	// 3. Check if the extra size required can be taken from spare size 
	// 3.1. If yes, simply update the md block
	if(extraSizeRequired <= fileMdPtr->spareSize){
		printf("[EXTEND] Adequate spare space in %s. Allocating...\n", fileName);
		fileMdPtr->spareSize -= extraSizeRequired;
		fileMdPtr->size += extraSizeRequired;
		print_md(fileMdPtr);
	// 3.2. If not, read all the file's contents in a buffer and delete it.
	//      Create a new file with the same name, write the buffer into it.      
	} else {
		printf("[EXTEND] No more spare space in %s. Deleting and re-creating...\n", fileName);
		// Here, we can actually simply invoke the move_to_partition function 
		// by making the from- and to- partition the same
		return recreate(fileName, newSize, ptnPtr, ptnPtr, ptnSize);
	}

	// Update modifiedAt time and return
	fileMdPtr->modifiedAt = time(NULL);
	return 0;
}
