#include "assignment3_helpers.h"

my_file_system* create_new_file_system(unsigned int s){
	my_file_system* ret = (my_file_system *) malloc(sizeof(my_file_system));
	ret->size = s;
	ret->first_partition = calloc(s, 1);
	ret->second_partition = calloc(s,1);
	return ret;
}

int save_partition(void* ptnPtr, int ptnSize, const char* fileName){
	FILE* filePtr = fopen(fileName, "w");
	int fileFd = fileno(filePtr);
	int bytesWritten = write(fileFd, ptnPtr, ptnSize);
	free(ptnPtr);
	printf("[SAVING PARTITION] Bytes written to file %s is: %d\n", fileName, bytesWritten);
	close(fileFd);
	return bytesWritten;
}

void* load_partition(int ptnSize, const char* fileName){
	void* newPtnPtr = calloc(ptnSize, 1);
	FILE* filePtr = fopen(fileName, "r");
	int fileFd = fileno(filePtr);
	int bytesRead = read(fileFd, newPtnPtr, ptnSize);
	printf("[LOADING PARTITION] Bytes read from file %s is: %d\n", fileName, bytesRead);
	close(fileFd);
	return newPtnPtr;
}

int create_file(const char* fileName, const int fileSize, 
	void* ptnPtr, int ptnSize){
	// Initialise offset that places the first byte of the new file
	int newFirstByteOffset;

	// 1. Count number of valid md blocks in md table
	int numOfValidFiles = count_valid_md_blocks(ptnPtr);

	// If there are no valid md blocks, ptn is "empty"
	// First byte starts at the end of md table 
	if (numOfValidFiles == 0){
		newFirstByteOffset = MD_TABLE_SIZE;
	// If there are valid blocks...
	} else {
		// 2. Create an array of sorted md_snaps. This is the "meta-metadata".
		md_snap* mdSnapsPtr = create_sorted_md_snaps(ptnPtr, numOfValidFiles);

		// 3. Traverse through mdSnaps array to find the appropriate 
		//	  place the first byte of the new file.
		// 3.1. First, check if there is space before the first file
		if(mdSnapsPtr[0].firstByteOffset - MD_TABLE_SIZE > fileSize){
			newFirstByteOffset = 0; 
		// 3.2. Then, check if there is space after the last file
		} else if (ptnSize - mdSnapsPtr[numOfValidFiles-1].lastByteOffset > fileSize){
			newFirstByteOffset = mdSnapsPtr[numOfValidFiles-1].lastByteOffset; 
		// 3.3. Check for spaces in the intervals between files
		} else {
			for(int i=0; i<numOfValidFiles; i++){
				int interval = mdSnapsPtr[i+1].firstByteOffset - mdSnapsPtr[i].lastByteOffset;
				if(interval > fileSize){
					newFirstByteOffset = mdSnapsPtr[i].lastByteOffset;
				}
			}
		}
		// 4. We know where to place the first byte, so we free mdSnaps
		//    as we don't need it anymore.
		free(mdSnapsPtr);
	} 	

	// 5. Find empty md block in the md table + fill it up with data
	md* newMdPtr = find_empty_md_block(ptnPtr);
	md* filledMdPtr = fill_md_block(newMdPtr, fileName, fileSize, newFirstByteOffset);
	return 0;
}

// Requires a type that can be 'w' or 'r'
file_handler* open_file(const char* fileName, void* ptnPtr, const char openType){
	md* matchingMdPtr = find_matching_md(ptnPtr, fileName);
	// Create a new fileHandler and return it
	file_handler* newFileHandlerPtr = malloc(sizeof(file_handler));
	newFileHandlerPtr->size = matchingMdPtr->size;
	newFileHandlerPtr->firstByteOffset = matchingMdPtr->firstByteOffset;
	newFileHandlerPtr->handlingOffset = matchingMdPtr->firstByteOffset;
	newFileHandlerPtr->type = openType;
	return newFileHandlerPtr;
}

int close_file(file_handler* fileHandlerPtr){
	free(fileHandlerPtr);
	return 0;
}

int delete_file(const char* fileName, void* ptnPtr){
	md* matchingMdPtr = find_matching_md(ptnPtr, fileName);
	assert(matchingMdPtr->valid == 1); // throw error if file not found
	matchingMdPtr->valid == 0;
	return 0;
}

void* read_file(file_handler* fileHandlerPtr, void* buffer, int numBytes, void* ptnPtr){
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
		printf("[READING FROM PTN FILE] Bytes read: %d\n", numBytes);
	// 3.2. If we only read remainingBytesToEOF
	} else {
		memcpy(buffer, parserPtr, remainingBytesToEOF);
		fileHandlerPtr->handlingOffset += remainingBytesToEOF;
		printf("[READING FROM PTN FILE] Bytes read: %d\n", remainingBytesToEOF);
	}

	// Return buffer that holds read data
	return buffer;
}

int write_file(file_handler* fileHandlerPtr, void* buffer, int numBytes, void* ptnPtr){
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
		printf("[WRITING TO PTN FILE] Bytes written: %d\n", numBytes);
    // 3.2. If numBytes is greater than remainingBytesToEOF, 
    //      we need to extend the file.
	} else {
		// EXTEND FILE, copy data...
		fileHandlerPtr->handlingOffset += numBytes;
		printf("[WRITING TO PTN FILE] Bytes written: ...\n");
	}
	return 0;
}


file_handler* rewind_handler(file_handler* fileHandlerPtr){
	fileHandlerPtr->handlingOffset = fileHandlerPtr->firstByteOffset;
	return fileHandlerPtr;
}


int main(void){	
	// Create file system
	my_file_system* myFileSysPtr = create_new_file_system(4096);

	// Create file
	create_file("a.txt", 512, myFileSysPtr->first_partition, myFileSysPtr->size);

	// Open the file and write to it
	char bufferForWritingA[50];
	strcpy(bufferForWritingA, "Hello");
	file_handler* myFileHandlerPtrAW = open_file("a.txt", myFileSysPtr->first_partition, 'w');
	write_file(myFileHandlerPtrAW, bufferForWritingA, 6, myFileSysPtr->first_partition);
	close_file(myFileHandlerPtrAW);

	// Save a partition into a text file
	save_partition(myFileSysPtr->first_partition, myFileSysPtr->size, "save.txt");
	// Load a partition from a text file
	myFileSysPtr->first_partition = load_partition(myFileSysPtr->size, "save.txt");

	// Open the same file and read from it
	char bufferForReadingA[50];
	file_handler* myFileHandlerPtrAR = open_file("a.txt", myFileSysPtr->first_partition, 'r');
	read_file(myFileHandlerPtrAR, bufferForReadingA, 6, myFileSysPtr->first_partition);
	printf("%s == Hello\n",bufferForReadingA);
	close_file(myFileHandlerPtrAR);

	// Create another file, open and write to it
	create_file("b.txt", 512, myFileSysPtr->first_partition, myFileSysPtr->size);
	char bufferForWritingB[50];
	strcpy(bufferForWritingB, "World");
	file_handler* myFileHandlerPtrBW = open_file("b.txt", myFileSysPtr->first_partition, 'w');
	write_file(myFileHandlerPtrBW, bufferForWritingB, 6, myFileSysPtr->first_partition);
	close_file(myFileHandlerPtrBW);

	defragment(myFileSysPtr->first_partition);
	list_files_in_order(myFileSysPtr->first_partition);

	return 0;
}