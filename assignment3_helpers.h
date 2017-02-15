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
md* fill_md_block(md* newMdPtr, const char* fileName, const int fileSize, const int newFirstByteOffset){
	md newMd = {
		.createdAt = time(NULL),
		.modifiedAt = time(NULL),
		.parentMd = NULL,
		.firstByteOffset = newFirstByteOffset,
		.size = fileSize,
		.rwef = 1110,
		.valid = 1
	};
	strcpy(newMd.name, fileName);
	printf("[NEW MD ENTRY] New file created as {name: %s, size: %d, firstByteOffset: %d}\n", newMd.name, newMd.size, newMd.firstByteOffset);
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
			mdSnapsPtr[snapsIdx].lastByteOffset = mdPtr[i].firstByteOffset + mdPtr[i].size;
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


// Given a filename, find a matching metadata block
md* find_matching_md(void* ptnPtr, const char* fileName){
	md* mdPtr = (md*) ptnPtr;
	for(int i=0; i<MAX_NUM_FILES; i++){
		char* currName = mdPtr[i].name;
		if(strcmp(fileName, currName) == 0 &&
			mdPtr[i].valid == 1){
			return &mdPtr[i]; 
		}
	}
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
// the handler's position and the EOF of a file.
int calculate_bytes_to_EOF(file_handler* fileHandlerPtr){
	return fileHandlerPtr->firstByteOffset + fileHandlerPtr->size - fileHandlerPtr->handlingOffset;
}


void list_files_in_order(void* ptnPtr){
	// Get partition metadata; an array of md_snaps
	int numOfValidFiles = count_valid_md_blocks(ptnPtr);
	md_snap* mdSnapsPtr = create_sorted_md_snaps(ptnPtr, numOfValidFiles);

	printf("[LISTING FILES IN ORDER]");
	for(int i=0; i<numOfValidFiles; i++){
		printf(" %s", mdSnapsPtr[i].name);
	}
	printf("\n");
}

// Defragments the entire partition
int defragment(void* ptnPtr){
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
		printf("[DEFRAG] Inspecting file: %s\n", mdSnapsPtr[i].name);
		// 1. Find the associated md block for the inspected file
		md* fileMdPtr = find_matching_md(ptnPtr, mdSnapsPtr[i].name);

		if(defragmenterOffset < mdSnapsPtr[i].firstByteOffset){
			
			printf("Matching filePtr name: %s, size: %d\n", fileMdPtr->name, fileMdPtr->size);
			// 2. Create ptr to the file's first byte
			char* fileFirstBytePtr = ((char*) ptnPtr) + mdSnapsPtr[i].firstByteOffset;
			// 3. Pull file data closer to the front of file storage
			memmove(defragmenterPtr, fileFirstBytePtr, fileMdPtr->size);
			printf("[DEFRAG] Pulling file %s forward from offset %d to offset %d\n", mdSnapsPtr[i].name, mdSnapsPtr[i].firstByteOffset, defragmenterOffset);
			// 4. Update the associated md block 
			fileMdPtr->firstByteOffset = defragmenterOffset;
			// 5. Update defragmenter and iterate...
			defragmenterOffset += fileMdPtr->size;
			defragmenterPtr += fileMdPtr->size;
		} else if (defragmenterOffset == mdSnapsPtr[i].firstByteOffset) {
			defragmenterOffset += fileMdPtr->size;
			defragmenterPtr += fileMdPtr->size;
		}
	}
	// Done using partition metadata, we free it
	free(mdSnapsPtr);
	return 0;
}

