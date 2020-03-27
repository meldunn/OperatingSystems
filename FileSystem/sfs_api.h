#include <stdbool.h>
#include <time.h>


//define constants
#define MAX_NUMBER_OF_FILES 200
#define NUMBER_OF_BLOCKS 1024
#define BLOCK_SIZE 1024
#define NUMBER_OF_INODE_BLOCKS 25
#define NUMBER_OF_FREE_BITMAP_BLOCKS 2
#define NUMBER_OF_DATA_BLOCKS 997
#define MAXFILENAME 20 //characters (including extension)
#define INODE_CACHE_SIZE 5 //(value can be chosen to optimize performance)
#define DISK_BLOCK_CACHE_SIZE 20 //(value can be chosen to optimize performance)
#define MAGIC 0xABCD0005


typedef struct
{
    unsigned int magic;
    unsigned int block_size;
    unsigned int file_system_size;
    unsigned int inode_table_length;
    unsigned int root_directory_inode;
} superblock;
typedef struct
{
    int map[NUMBER_OF_BLOCKS]; // Implementation of bool using 1 byte
} free_block_bitmap;
typedef struct
{
    char filename[MAXFILENAME];
    unsigned int inode_number;
} file_entry;
typedef struct
{
    file_entry directory[MAX_NUMBER_OF_FILES];
    int current_index;
    time_t last_modified;
} directory_table;
typedef struct
{
    unsigned int mode; //read, write...
    unsigned int link_cnt;
    unsigned int uid;
    unsigned int gid;
    unsigned int size; //num bytes in file
    unsigned int block_pointer[12];
    unsigned ind_pointer;
} inode;
typedef struct
{
    inode cached_inodes[INODE_CACHE_SIZE]; // Constant equal to number of inodes
} inode_cache;
typedef struct
{
    int avail[MAX_NUMBER_OF_FILES]; 
} free_inodes_bitmap;
typedef struct
{
    unsigned int i_node;
    unsigned int read_ptr;
    unsigned int write_ptr;
} file_descriptor;
typedef struct
{
    file_descriptor fd[MAX_NUMBER_OF_FILES];
    int next_avail;
} fd_table;
typedef struct {
    int ind_block[BLOCK_SIZE/(sizeof(int))];
} indirect_block_pointer;


//typedef struct
// {
//     unsigned int address[DISK_BLOCK_CACHE_SIZE];
//     block cached[DISK_BLOCK_CACHE_SIZE];
//     bool modified[DISK_BLOCK_CACHE_SIZE]; // Implementation of bool using 1 byte
// } disk_block_cache;

void mksfs(int fresh); //creates the file system
int sfs_getnextfilename(char *fname); //get the name of the next file in directory
int sfs_getfilesize(const char* path); //get the size of the given file
int sfs_fopen(char *name); //opens the given file
int sfs_fclose(int fileID); //closes the given file
int sfs_frseek(int fileID, int loc); // seek (Read) to the location from beginning
int sfs_fwseek(int fileID,int loc); // seek (Write) to the location from beginning
int sfs_fwrite(int fileID,char *buf, int length); // write buf characters into disk
int sfs_fread(int fileID,char *buf, int length); // read characters from disk into buf
int sfs_remove(char *file);// removes a file from the filesystem

