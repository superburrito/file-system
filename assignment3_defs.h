// Type definitions
typedef struct {
	void* first_partition;
	void* second_partition;
	unsigned int size;
} my_file_system;

typedef struct {
	char name[16];
	time_t createdAt;
	time_t modifiedAt;
	void* parentMd;
	int firstByteOffset;
	int size;
	int spareSize;
	unsigned short rwef;
	unsigned short valid;
} md;

typedef struct {
	char name[16];
	int firstByteOffset;
	int lastByteOffset;
} md_snap;

// File handlers can only either READ or WRITE. 
// If user is reading, handlingOffset refers to the current 
// position of where the user is reading.
// If user is writing, handlingOffset refers to the current
// position of where the user is writing.
typedef struct {
	int size;
	int firstByteOffset;
	int handlingOffset;
	char type;
} file_handler;


// Macros
#define MAX_NUM_FILES		10 // up to user
#define MD_TABLE_SIZE 		(sizeof(md) * MAX_NUM_FILES) // 560
#define EXPECTED_PTN_SIZE	8192 // has to be greater than MD_TABLE_SIZE
#define SPARE_SIZE_PROPORTION 5 // up to user. Spare size = 1/n of actual size


// Function prototypes