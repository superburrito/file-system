#include <stdio.h> // fopen, fclose
#include <stdlib.h> // qsort
#include <string.h> // %s, strcmp, strcpy
#include <time.h> // time_t
#include <unistd.h> // close
#include <fcntl.h> // read, write
#include <assert.h> // assert
#include "assignment3_defs.h" // md, md_snap, my_file_system

// Get the number of valid files
// i.e. number of valid metadata (md) blocks
int count_valid_md_blocks(void* ptnPtr){
	int numOfValidFiles = 0;
	md* mdPtr = (md*) ptnPtr;
	for(int i=0; i<MAX_NUM_FILES; i++){
		if(mdPtr[i].valid == 1){
			numOfValidFiles++;	
		}
	}
	return numOfValidFiles;
}

// Compare function to sort md_snaps
int cmp_func (const void* a, const void* b){
	md_snap* snapA = (md_snap*) a;
	md_snap* snapB = (md_snap*) b;
	return snapA->firstByteOffset - snapB->firstByteOffset;
}

// Find an empty md block in the md table
md* find_empty_md_block(void* ptnPtr){
	md* mdPtr = (md*) ptnPtr;
	for(int i=0; i<MAX_NUM_FILES; i++){
		if(mdPtr[i].valid == 0){
			return &mdPtr[i]; 
		}
	}
}


// Fill up a md block with fresh data
md* fill_md_block(md* newMdPtr, char* fileName, int fileSize, int newFirstByteOffset, md* immedParentMdPtr, int rwef){
	md newMd = {
		.createdAt = time(NULL),
		.modifiedAt = time(NULL),
		.parentMdPtr = immedParentMdPtr,
		.firstByteOffset = newFirstByteOffset,
		.size = fileSize,
		.rwef = rwef,
		.valid = 1
	};
	// If folder, no need for spare space
	if(rwef % 10 == 1){
		newMd.spareSize = 0; 
	} else {
		newMd.spareSize = fileSize/SPARE_SIZE_PROPORTION;
	}
	strcpy(newMd.name, fileName);
	printf("[MD NEW] New file created as {name: %s, size: %d (+%d), firstByteOffset: %d, parentName(deferenced): %s}\n", newMd.name, newMd.size, newMd.spareSize, newMd.firstByteOffset, newMd.parentMdPtr->name);
	return memcpy(newMdPtr, &newMd, sizeof(md));
}

// Get a snapshot of the entire partition by creating
// an array of elements of type md_snap. Each md_snap
// holds a filename and, more importantly, the starting
// and ending position (referred to as firstByteOffset 
// and lastByteOffset) of a file. 
md_snap* create_sorted_md_snaps(md* ptnPtr, int numOfValidFiles){
	md* mdPtr = (md*) ptnPtr;
	md_snap* mdSnapsPtr = malloc(numOfValidFiles * sizeof(md_snap));

	int snapsIdx = 0;
	for(int i=0; i<MAX_NUM_FILES; i++){
		if(mdPtr[i].valid == 1){
			strcpy(mdSnapsPtr[snapsIdx].name, mdPtr[i].name);
			mdSnapsPtr[snapsIdx].firstByteOffset = mdPtr[i].firstByteOffset;
			mdSnapsPtr[snapsIdx].lastByteOffset = mdPtr[i].firstByteOffset + mdPtr[i].size + mdPtr[i].spareSize;
			snapsIdx++;
		}
	}
	// Total number of of md_snaps created should be, of course,
	// equal to the number of valid md blocks. Throw error if otherwise.
	assert(snapsIdx == numOfValidFiles); 

	// Sort mdSnaps by their starting positions (i.e. firstByteOffsets)
	qsort(mdSnapsPtr, numOfValidFiles, sizeof(md_snap), cmp_func);

	return mdSnapsPtr;
}


// Given a filename, find a valid metadata block that matches
md* find_matching_md(void* ptnPtr, char* fileName){
	int matchingMdFound = 0;
	md* mdPtr = (md*) ptnPtr;
	for(int i=0; i<MAX_NUM_FILES; i++){
		if(strcmp(fileName, mdPtr[i].name) == 0 &&
			mdPtr[i].valid == 1){
			matchingMdFound++;
			printf("[MD LOOKUP] Found %s.\n", fileName);
			return &mdPtr[i]; 
		}
	}
	printf("[MD LOOKUP] WARNING -- Metadata associated with %s not found.\n", fileName);
}

// Given a filename, count the number of valid metadata blocks that match.
// The result should always be either 0 or 1
int count_matching_md(void* ptnPtr, char* fileName){
	int matchingMdFound = 0;
	md* mdPtr = (md*) ptnPtr;
	for(int i=0; i<MAX_NUM_FILES; i++){
/*		printf("[MD LOOKUP] Comparing %s with %s whose validity is %d gives %d\n", fileName, mdPtr[i].name, mdPtr[i].valid, strcmp(fileName, mdPtr[i].name));*/
		if(strcmp(fileName, mdPtr[i].name) == 0 &&
			mdPtr[i].valid == 1){
			matchingMdFound++;
		}
	}
	assert(matchingMdFound == 0 || matchingMdFound == 1);
	printf("[MD VERIFY] %d instance(s) of %s detected.\n", matchingMdFound, fileName);
	return matchingMdFound;
}

void ensure_file_exists(void* ptnPtr, char* fileName){
	assert(count_matching_md(ptnPtr, fileName) == 1);
}

// Helpers for reading and writing
// Give a fileHandler, provide a pointer to the handler's 
// curr position
char* convert_offset_to_curr_pos_ptr(void* ptnPtr, file_handler* fileHandlerPtr){
	char* parserPtr = (char*) ptnPtr;
	parserPtr += fileHandlerPtr->handlingOffset;
	return parserPtr;
}

// Given a fileHandler, calculate the remaining bytes between
// the handler's position and the EOF of a file (excludes spare size!).
int calculate_bytes_to_EOF(file_handler* fileHandlerPtr){
	return fileHandlerPtr->firstByteOffset + fileHandlerPtr->size - fileHandlerPtr->handlingOffset;
}


void list_files_in_order(void* ptnPtr){
	// Get partition metadata; an array of md_snaps
	int numOfValidFiles = count_valid_md_blocks(ptnPtr);
	md_snap* mdSnapsPtr = create_sorted_md_snaps(ptnPtr, numOfValidFiles);

	printf("[LIST] Ordered list of files: ");
	for(int i=0; i<numOfValidFiles; i++){
		printf(" %s", mdSnapsPtr[i].name);
	}
	printf("\n");
}

void print_md(md* mdPtr){
	printf("[MD UPDATE] File is now {name: %s, size: %d (+%d), firstByteOffset: %d}\n", mdPtr->name, mdPtr->size, mdPtr->spareSize, mdPtr->firstByteOffset);
}

// Defragments the entire partition
int defragment(void* ptnPtr){
	printf("[DEFRAG] Defragmenting...\n");
	// Get partition metadata; an array of md_snaps
	int numOfValidFiles = count_valid_md_blocks(ptnPtr);
	md_snap* mdSnapsPtr = create_sorted_md_snaps(ptnPtr, numOfValidFiles);

	// Create a defragmenter that will shut any empty space directly
	// in front of it by "pulling" the closest file in front.
	// Defragmentation starts from the end of the md table.
	char* defragmenterPtr = ((char*) ptnPtr) + MD_TABLE_SIZE;
	int defragmenterOffset = MD_TABLE_SIZE;
	// Iterate through each mdSnap (i.e. each valid file)
	for(int i=0; i<numOfValidFiles; i++){
		// TEST: defragmenterOffset CANNOT be greater than the first byte 
		// of the currently examined file OR the size of the ptn!
		assert(defragmenterOffset <= mdSnapsPtr[i].firstByteOffset);
		printf("[DEFRAG] Inspecting %s...\n", mdSnapsPtr[i].name);
		// 1. Find the associated md block for the inspected file
		md* fileMdPtr = find_matching_md(ptnPtr, mdSnapsPtr[i].name);

		if(defragmenterOffset < mdSnapsPtr[i].firstByteOffset){
			
			printf("[DEFRAG] Pull required. Expanded file is: {name: %s, size: %d (+%d)}\n", fileMdPtr->name, fileMdPtr->size, fileMdPtr->spareSize);
			// 2. Create ptr to the file's first byte
			char* fileFirstBytePtr = ((char*) ptnPtr) + mdSnapsPtr[i].firstByteOffset;
			// 3. Pull file data closer to the front of file storage
			memmove(defragmenterPtr, fileFirstBytePtr, fileMdPtr->size + fileMdPtr->spareSize);
			printf("[DEFRAG] Pulling file %s forward from offset %d to offset %d\n", mdSnapsPtr[i].name, mdSnapsPtr[i].firstByteOffset, defragmenterOffset);
			// 4. Update the associated md block 
			fileMdPtr->firstByteOffset = defragmenterOffset;
			// 5. Update defragmenter and iterate...
			defragmenterOffset += fileMdPtr->size + fileMdPtr->spareSize;
			defragmenterPtr += fileMdPtr->size + fileMdPtr->spareSize;
		} else if (defragmenterOffset == mdSnapsPtr[i].firstByteOffset) {
			defragmenterOffset += fileMdPtr->size + fileMdPtr->spareSize;
			defragmenterPtr += fileMdPtr->size + fileMdPtr->spareSize;
		}
	}
	// Done using partition metadata, so we free it
	free(mdSnapsPtr);
	return 0;
}

int calculate_free_bytes(void* ptnPtr, int ptnSize){
	md* mdPtr = (md*) ptnPtr;
	int bytesUsed = MD_TABLE_SIZE;
	for(int i=0; i<MAX_NUM_FILES; i++){
		if(mdPtr[i].valid == 1){
			bytesUsed += mdPtr[i].size + mdPtr[i].spareSize;
		}
	}
	int bytesFree = ptnSize - bytesUsed;
	printf("[DISK SPACE] %d free bytes are left in this partition.\n", bytesFree);
	return bytesFree;
}


// Helper that relies on primary functions
// Recreates a file by storing its data, deleting it and re-creating it.
// Invoked by move_to_partition and sometimes, extend_file
int recreate(char* fileName, int recreatedSize, void* fromPtnPtr, void* toPtnPtr, int ptnSize){
	printf("[RE-CREATE] Recreating %s...\n", fileName);
	// 1. Reading and Copying
	md* fileMdPtr = find_matching_md(fromPtnPtr, fileName);
	int copySize = fileMdPtr->size;
	int copyRwef = fileMdPtr->rwef;
	char buffer[copySize];
	file_handler* fileHandlerForReading = open_file(fileName, fromPtnPtr, 'r');
	read_file(fileHandlerForReading, buffer, copySize, fromPtnPtr);
	close_file(fileHandlerForReading);
	// 2. Deleting
	delete_file(fileName, fromPtnPtr);
	// 3. Creating
	create_file(fileName, recreatedSize, toPtnPtr, ptnSize, copyRwef);
	// 4. Writing
	file_handler* fileHandlerForWriting = open_file(fileName, toPtnPtr, 'w');
	write_file(fileHandlerForWriting, buffer, copySize, toPtnPtr, ptnSize);
	close_file(fileHandlerForWriting);
	return 0;
}


// Give a filename, determines how many parents the file should have
int count_parents(const char* fileName){
	int ctr = 0;
	for(int i=0; i<strlen(fileName); i++){
		if(fileName[i] == '/'){
			ctr++;
		}
	}
	return ctr -1;
}

// Given a filename, checks if all parents above it exist.
void check_parents(const char* fileName, void* ptnPtr, int numOfParents){
	// strtok will modify the string passed to it, 
	// so we make a copy of fileName  
	char fileNameCopy[strlen(fileName)];
	memset(fileNameCopy, '\0', strlen(fileName));
	strcpy(fileNameCopy, fileName);

	// Use a large parent dir buffer for us verify every parent 
	// This buffer will "grow" from '/fd1' -> '/fd1/fd2' -> ...	
	char parentNameBuffer[strlen(fileName)];
	memset(parentNameBuffer, '\0', strlen(fileName));
	
	// Using strtok, concat each token to parentNameBuffer
	// and inspect that string 	
	char* token = strtok(fileNameCopy, "/");
	int ctr = 0;
	while(token != NULL && ctr < numOfParents){ // should use <, not <= 
		strcat(parentNameBuffer, "/");
		strcat(parentNameBuffer, token);
		ensure_file_exists(ptnPtr, parentNameBuffer);
		token = strtok(NULL, "/");
		ctr++;
	}
}


// Given a filename and a buffer, 
// updates the buffer with the parent's path
void get_parent_path(const char* fileName, char* parentNameBuffer, int numOfParents){
	// strtok will modify the string passed to it, 
	// so we make a copy of fileName  
	char fileNameCopy[strlen(fileName)];
	memset(fileNameCopy, '\0', strlen(fileName));
	strcpy(fileNameCopy, fileName);

	// Ensure that parentNameBuffer is "clean", no unwanted data
	memset(parentNameBuffer, '\0', strlen(fileName));

	// Using strtok, concat each token to parentNameBuffer 
	char* token = strtok(fileNameCopy , "/");
	int ctr = 0;
	while(token != NULL && ctr < numOfParents){ // should use <, not <= 
		strcat(parentNameBuffer, "/");
		strcat(parentNameBuffer, token);
		token = strtok(NULL, "/");
		ctr++;
	}

	printf("[DIR CHECK] Immediate parent of %s is %s\n", fileName, parentNameBuffer); 
}

// Takes /f1/f2/a.txt and returns a.txt
void get_last_path_segment(const char* fileName, char* lastPathSegmentBuffer){
	// strtok will modify the string passed to it, 
	// so we make a copy of fileName  
	char fileNameCopy[strlen(fileName)];
	memset(fileNameCopy, '\0', strlen(fileName));
	strcpy(fileNameCopy, fileName);

	// Ensure that lastPathSegmentBuffer is "clean", no unwanted data
	memset(lastPathSegmentBuffer, '\0', MAX_FILE_NAME_SIZE);
	// Count parents
	int numOfParents = count_parents(fileName);
	// Keep strtok-ing until last segment 
	char* token = strtok(fileNameCopy, "/");
	int ctr = 0;
	while(token != NULL){
		if(ctr == numOfParents){
			strcpy(lastPathSegmentBuffer, token);
		}  
		token = strtok(NULL, "/");
		ctr++;
	}
	printf("[DIR CHECK] Last path segment of %s is %s\n", fileName, lastPathSegmentBuffer);
}

// Adds a child's metadata ptr inside a folder's internal storage
void add_child_to_folder(char* folderName, void* ptnPtr, md* childMdPtr){
	file_handler* folderHandlerPtr = open_file(folderName, ptnPtr, 'w');
	char* charParserPtr = convert_offset_to_curr_pos_ptr(ptnPtr, folderHandlerPtr);
	md** mdPtrParserPtr = (md**) charParserPtr; 
	for(int i = 0; i < MAX_NUM_FILES; i++){
		if(mdPtrParserPtr[i] == NULL || mdPtrParserPtr[i]->valid == 0){
			mdPtrParserPtr[i] = childMdPtr;
			break;
		}		
	}
	close_file(folderHandlerPtr);
}

// Removes a child's metadata inside a folder's internal storage
void remove_child_from_folder(char* folderName, void* ptnPtr, md* childMdPtr){
	file_handler* folderHandlerPtr = open_file(folderName, ptnPtr, 'w');
	char* charParserPtr = convert_offset_to_curr_pos_ptr(ptnPtr, folderHandlerPtr);
	md** mdPtrParserPtr = (md**) charParserPtr; 
	for(int i = 0; i < MAX_NUM_FILES; i++){
		if(mdPtrParserPtr[i] != NULL &&
		   mdPtrParserPtr[i]->valid == 1 &&
		   mdPtrParserPtr[i]->name == childMdPtr->name){
			mdPtrParserPtr[i] = NULL;
			break;
		}		
	}
	close_file(folderHandlerPtr);
}

// Nullifies all valid metadata ptrs inside a folder's internal storage 
void delete_children_in_folder(char* folderName, void* ptnPtr){
	file_handler* folderHandlerPtr = open_file(folderName, ptnPtr, 'w');
	char* charParserPtr = convert_offset_to_curr_pos_ptr(ptnPtr, folderHandlerPtr);
	md** mdPtrParserPtr = (md**) charParserPtr;
	for(int i = 0; i < MAX_NUM_FILES; i++){
		if(mdPtrParserPtr[i] != NULL && mdPtrParserPtr[i]->valid == 1){
			delete_file(mdPtrParserPtr[i]->name, ptnPtr);
			mdPtrParserPtr[i] = NULL;
		}
	}
}
/*
void move_children_in_folder(char* folderName, void* fromPtnPtr, void* toPtnPtr, int ptnSize){
	file_handler* folderHandlerPtr = open_file(folderName, fromPtnPtr, 'w');
	char* charParserPtr = convert_offset_to_curr_pos_ptr(fromPtnPtr, folderHandlerPtr);
	md** mdPtrParserPtr = (md**) charParserPtr;
	for(int i = 0; i < MAX_NUM_FILES; i++){
		if(mdPtrParserPtr[i] != NULL && mdPtrParserPtr[i]->valid == 1){
			move_to_partition(mdPtrParserPtr[i]->name, fromPtnPtr, toPtnPtr, ptnSize);
		}
	}
}*/