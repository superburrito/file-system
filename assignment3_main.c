#include "assignment3_primary.h"


int main(void){	
	printf("\n");
	printf("\n");
	printf("\n");
	printf("\n");
	printf("\n");

	// Create file system
	my_file_system* myFileSysPtr = create_new_file_system(EXPECTED_PTN_SIZE);
	
	// Create folder
	printf("\n");
	printf("***** CREATING AND WRITING INTO f1/f2/a.txt *****\n");
	create_folder("/f1", myFileSysPtr->first_partition, myFileSysPtr->size, 1111);
	create_folder("/f1/f2", myFileSysPtr->first_partition, myFileSysPtr->size, 1111);
	create_file("/f1/f2/a.txt", 512, myFileSysPtr->first_partition, myFileSysPtr->size, 1110);
	// Open the file and write to it
	char bufferForWritingA[50];
	strcpy(bufferForWritingA, "Hello");
	file_handler* myFileHandlerPtrAW = open_file("/f1/f2/a.txt", myFileSysPtr->first_partition, 'w');
	write_file(myFileHandlerPtrAW, bufferForWritingA, 6, myFileSysPtr->first_partition, myFileSysPtr->size);
	close_file(myFileHandlerPtrAW);

	// Save a partition into a text file
	printf("\n");
	printf("***** SAVING AND LOADING A PARTITION *****\n");
	save_partition(myFileSysPtr->first_partition, myFileSysPtr->size, "save.txt");
	// Load a partition from a text file
	myFileSysPtr->first_partition = load_partition(myFileSysPtr->size, "save.txt");

	// Open the same file and read from it
	printf("\n");
	printf("***** READING FROM f1/f2/a.txt *****\n");
	char bufferForReadingA[50];
	file_handler* myFileHandlerPtrAR = open_file("/f1/f2/a.txt", myFileSysPtr->first_partition, 'r');
	read_file(myFileHandlerPtrAR, bufferForReadingA, 6, myFileSysPtr->first_partition);
	printf("String %s should be 'Hello'\n",bufferForReadingA);
	close_file(myFileHandlerPtrAR);
	list_files_in_order(myFileSysPtr->first_partition);

	// Create another file, open and write to it
	printf("\n");
	printf("***** CREATING AND WRITING TO /b.txt *****\n");
	create_file("/b.txt", 1024, myFileSysPtr->first_partition, myFileSysPtr->size, 1110);
	char bufferForWritingB[50];
	strcpy(bufferForWritingB, "World");
	file_handler* myFileHandlerPtrBW = open_file("/b.txt", myFileSysPtr->first_partition, 'w');
	write_file(myFileHandlerPtrBW, bufferForWritingB, 6, myFileSysPtr->first_partition, myFileSysPtr->size);
	close_file(myFileHandlerPtrBW);

	printf("\n");
	printf("***** DEFRAGMENTING *****\n");
	defragment(myFileSysPtr->first_partition);
	list_files_in_order(myFileSysPtr->first_partition);
	
	printf("\n");
	printf("***** EXTENDING /b.txt TWICE *****\n");
	extend_file("/b.txt", 1100, myFileSysPtr->first_partition, myFileSysPtr->size);
	extend_file("/b.txt", 1500, myFileSysPtr->first_partition, myFileSysPtr->size);


	printf("\n");
	printf("***** MOVING A FILE TO ANOTHER PTN *****\n");
	move_to_partition("/b.txt", myFileSysPtr->first_partition, myFileSysPtr->second_partition, myFileSysPtr->size);
	list_files_in_order(myFileSysPtr->first_partition);
	list_files_in_order(myFileSysPtr->second_partition);


	printf("\n");
	printf("***** REMOVING FOLDER /f1 *****\n");
	delete_file("/f1", myFileSysPtr->first_partition);
	list_files_in_order(myFileSysPtr->first_partition);

	return 0;
}