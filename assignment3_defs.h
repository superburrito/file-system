// Macros I
#define MAX_FILE_NAME_SIZE		16
#define EXPECTED_PTN_SIZE		4096 // has to be greater than MD_TABLE_SIZE
#define SPARE_SIZE_PROPORTION	5 // up to user. Spare size = 1/n of actual size
#define MAX_NUM_FILES			10 // up to user


// Type definitions
typedef struct {
	void* first_partition;
	void* second_partition;
	unsigned int size;
} my_file_system;

typedef struct md md;

struct md {
	char name[MAX_FILE_NAME_SIZE];
	time_t createdAt;
	time_t modifiedAt;
	md* parentMdPtr;
	int firstByteOffset;
	int size;
	int spareSize;
	unsigned short rwef;
	unsigned short valid;
};

typedef struct {
	char name[MAX_FILE_NAME_SIZE];
	int firstByteOffset;
	int lastByteOffset;
} md_snap;

// File handlers can only either READ or WRITE. 
// If user is reading, handlingOffset refers to the current 
// position of where the user is reading.
// If user is writing, handlingOffset refers to the current
// position of where the user is writing.
typedef struct {
	char name[MAX_FILE_NAME_SIZE];
	int size;
	int firstByteOffset;
	int handlingOffset;
	char type;
} file_handler;


// Macros II
#define MD_TABLE_SIZE			(sizeof(md) * MAX_NUM_FILES) // 560
#define MAX_FOLDER_SIZE			(sizeof(md*) * MAX_NUM_FILES) // 80


// Prototypes of Simple Helper Functions
int count_valid_md_blocks(void* ptnPtr);
int cmp_func (const void* a, const void* b);
md* find_empty_md_block(void* ptnPtr);
md* fill_md_block(md* newMdPtr, char* fileName, int fileSize, int newFirstByteOffset, md* immedParentMdPtr, int rwef);
md_snap* create_sorted_md_snaps(md* ptnPtr, int numOfValidFiles);
md* find_matching_md(void* ptnPtr, char* fileName);
int count_matching_md(void* ptnPtr, char* fileName);
void ensure_file_exists(void* ptnPtr, char* fileName);
char* convert_offset_to_curr_pos_ptr(void* ptnPtr, file_handler* fileHandlerPtr);
int calculate_bytes_to_EOF(file_handler* fileHandlerPtr);
void list_files_in_order(void* ptnPtr);
void print_md(md* mdPtr);
int defragment(void* ptnPtr);
int calculate_free_bytes(void* ptnPtr, int ptnSize);
int count_parents(const char* fileName);
void check_parents(const char* fileName, void* ptnPtr, int numOfParents);
void get_parent_path(const char* fileName, char* parentNameBuffer, int numOfParents);
void get_last_path_segment(const char* fileName, char* lastPathSegmentBuffer);



// Prototypes of Complex Helper Functions
int recreate(char* fileName, int recreatedSize, void* ptnPtr, int ptnSize);
int copy(char* fileName, void* fromPtnPtr, void* toPtnPtr, int ptnSize);
void copy_children(char* folderName, void* fromPtnPtr, void* toPtnPtr, int ptnSize);
void add_child_to_folder(char* folderName, void* ptnPtr, md* childMdPtr);
void remove_child_from_folder(char* folderName, void* ptnPtr, md* childMdPtr);
void delete_children_in_folder(char* folderName, void* ptnPtr);
void new_parent_for_children(char* folderName, char* fromParentName, char* toParentName, void* ptnPtr);
void move_children_to_root(char* folderName, void* ptnPtr);


// Prototypes of Primary Functions
my_file_system* create_new_file_system(unsigned int s);
int save_partition(void* ptnPtr, int ptnSize, char* fileName);
void* load_partition(int ptnSize, char* fileName);
int create_file(char* fileName, int fileSize, void* ptnPtr, int ptnSize, int rwef);
file_handler* open_file(char* fileName, void* ptnPtr, char openType);
int close_file(file_handler* fileHandlerPtr);
int delete_file(char* fileName, void* ptnPtr);
void* read_file(file_handler* fileHandlerPtr, void* buffer, int numBytes, void* ptnPtr);
int write_file(file_handler* fileHandlerPtr, void* buffer, int numBytes, void* ptnPtr, int ptnSize);
int move_to_partition(char* fileName, void* fromPtnPtr, void* toPtnPtr, int ptnSize);
int extend_file(char* fileName, int newSize, void* ptnPtr, int ptnSize);
int new_parent(char* fileName, char* fromParentName, char* toParentName, void* ptnPtr);
int move_to_root(char* fileName, char* fromParentName, void* ptnPtr);


