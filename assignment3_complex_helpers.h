#include "assignment3_simple_helpers.h"

// More powerful helpers that rely on some main functions reading/writing (i.e. open_file, close_file, read_file, write_file)

// Recreates a file by saving its data, deleting it, re-creating it and then loading the data. Used for expand file sizes. Invoked sometimes by extend_file.
int recreate(char* fileName, int recreatedSize, void* ptnPtr, int ptnSize){
	printf("[RE-CREATE] Recreating %s...\n", fileName);
	// 1. Reading and Copying
	md* fileMdPtr = find_matching_md(ptnPtr, fileName);
	int copySize = fileMdPtr->size;
	int copyRwef = fileMdPtr->rwef;
	char buffer[copySize];
	file_handler* fileHandlerForReading = open_file(fileName, ptnPtr, 'r');
	read_file(fileHandlerForReading, buffer, copySize, ptnPtr);
	close_file(fileHandlerForReading);
	// 2. Deleting
	delete_file(fileName, ptnPtr);
	// 3. Creating
	create_file(fileName, recreatedSize, ptnPtr, ptnSize, copyRwef);
	// 4. Writing
	file_handler* fileHandlerForWriting = open_file(fileName, ptnPtr, 'w');
	write_file(fileHandlerForWriting, buffer, copySize, ptnPtr, ptnSize);
	close_file(fileHandlerForWriting);
	return 0;
}

// Copies a file to a new partition by storing its data, creating it and loading the data. Invoked by move_to_partition.
int copy(char* fileName, void* fromPtnPtr, void* toPtnPtr, int ptnSize){
	printf("[COPY] Copying %s...\n", fileName);
	// 1. Reading and Copying
	md* fileMdPtr = find_matching_md(fromPtnPtr, fileName);
	int copySize = fileMdPtr->size;
	int copyRwef = fileMdPtr->rwef;
	char buffer[copySize];
	file_handler* fileHandlerForReading = open_file(fileName, fromPtnPtr, 'r');
	read_file(fileHandlerForReading, buffer, copySize, fromPtnPtr);
	close_file(fileHandlerForReading);
	// 2. Creating
	create_file(fileName, copySize, toPtnPtr, ptnSize, copyRwef);
	// 3. Writing
	file_handler* fileHandlerForWriting = open_file(fileName, toPtnPtr, 'w');
	write_file(fileHandlerForWriting, buffer, copySize, toPtnPtr, ptnSize);
	close_file(fileHandlerForWriting);
	return 0;
}


void copy_children(char* folderName, void* fromPtnPtr, void* toPtnPtr, int ptnSize){
	file_handler* folderHandlerPtr = open_file(folderName, fromPtnPtr, 'w');
	char* charParserPtr = convert_offset_to_curr_pos_ptr(fromPtnPtr, folderHandlerPtr);
	md** mdPtrParserPtr = (md**) charParserPtr; 
	for(int i = 0; i < MAX_NUM_FILES; i++){
		if(mdPtrParserPtr[i] != NULL && mdPtrParserPtr[i]->valid == 1){
			copy(mdPtrParserPtr[i]->name, fromPtnPtr, toPtnPtr, ptnSize);
		}		
	}
	close_file(folderHandlerPtr);
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
	close_file(folderHandlerPtr);
}

// Updates parents for all children
void new_parent_for_children(char* folderName, char* fromParentName, char* toParentName, void* ptnPtr){
	file_handler* folderHandlerPtr = open_file(folderName, ptnPtr, 'w');
	char* charParserPtr = convert_offset_to_curr_pos_ptr(ptnPtr, folderHandlerPtr);
	md** mdPtrParserPtr = (md**) charParserPtr;
	for(int i = 0; i < MAX_NUM_FILES; i++){
		if(mdPtrParserPtr[i] != NULL && mdPtrParserPtr[i]->valid == 1){
			new_parent(mdPtrParserPtr[i]->name, fromParentName, toParentName, ptnPtr);
		}
	}
	close_file(folderHandlerPtr);
}

// Sets parent to root for all children
void move_children_to_root(char* folderName, void* ptnPtr){
	file_handler* folderHandlerPtr = open_file(folderName, ptnPtr, 'w');
	char* charParserPtr = convert_offset_to_curr_pos_ptr(ptnPtr, folderHandlerPtr);
	md** mdPtrParserPtr = (md**) charParserPtr;
	for(int i = 0; i < MAX_NUM_FILES; i++){
		if(mdPtrParserPtr[i] != NULL && mdPtrParserPtr[i]->valid == 1){
			move_to_root(mdPtrParserPtr[i]->name, folderName, ptnPtr);
		}
	}
	close_file(folderHandlerPtr);
}

