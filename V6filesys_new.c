#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

// CONSTANTS
#define BLOCK_SIZE 1024
#define INODE_SIZE 64
#define inode_alloc 0100000 //Flag value to allocate an inode
#define pfile 000000        //To define file as a plain file
#define lfile 010000        //To define file as a large file
#define directory 040000    //To define file as a Directory
#define max_array 512
#define null_size 1024 // * check where it's getting used
#define FREE_BLOCKS 251
#define DIRECTORY_SIZE 32

// VARIABLES
int file_handle;
unsigned short block_number_tracker[max_array];

// super block
typedef struct {
int isize;
int fsize;
int nfree;
unsigned int free[251]; char flock;
char ilock;
char fmod; unsigned int time;
} superblock_type;
superblock_type super;

// inode 
typedef struct {
unsigned short flags;
unsigned short nlinks;
unsigned int uid;
unsigned int gid;
unsigned int size0;
unsigned int size1;
unsigned int addr[9];
unsigned int actime;
unsigned int modtime;
} inode_type;
inode_type first_inode;

// directory
typedef struct { 
    unsigned int inode; 
    char filename[28];
} dir_type;

dir_type new_directory;


void chaining_blocks(unsigned int total_blocks, unsigned int inode_block_count);
void create_root_dir();
int initialize_file_system(char *path, unsigned int total_blocks, unsigned int total_inodes_blocks);
unsigned int get_free_block();
void read_block(unsigned short *dest, unsigned short bno);
void write_block(unsigned int *dest, unsigned int bno);
void write_inode(inode_type current_inode, unsigned int inode_index);

//in order

// Data blocks chaining procedure
void chaining_blocks(unsigned int total_blocks, unsigned int inode_block_count)
{
    unsigned int free_block_numbers[FREE_BLOCKS + 1];

    unsigned int emptybuffer[FREE_BLOCKS + 1];

    unsigned int linked_blockcount;

    unsigned int data_block_chunk_index;

    unsigned int data_blocks_count = (total_blocks - inode_block_count - 2);

    unsigned int split_blocks =  data_blocks_count / FREE_BLOCKS;           //TODO check 
    unsigned short extra_blocks = data_blocks_count % FREE_BLOCKS;
    unsigned short idx = 0;

    lseek(file_handle, total_blocks * BLOCK_SIZE -1, SEEK_SET);
    write(file_handle, 0, 1);

    int i = 0;
    for (idx = 0; idx <= FREE_BLOCKS; idx++)
    {
        emptybuffer[idx] = 0;
        free_block_numbers[idx] = 0;
        block_number_tracker[idx] = 0;
        
        // free block numbers in free array
        if (idx < FREE_BLOCKS){
            super.free[super.nfree] = idx + 2 + inode_block_count;
            ++super.nfree;
        }
    }
    super.free[0] = 2 + inode_block_count + FREE_BLOCKS;

    for (data_block_chunk_index = 0 ; data_block_chunk_index < split_blocks; data_block_chunk_index ++){

        free_block_numbers[0] = FREE_BLOCKS;

        // get the block where the next free block addresses will be stored

        if (extra_blocks == 0 && data_block_chunk_index == split_blocks - 1){
            free_block_numbers[1] = 0 ;                 
        }else{
            free_block_numbers[1] = inode_block_count + 2 + (data_block_chunk_index + 1) * FREE_BLOCKS ;
        }
        
        
        for (i = 2; i < FREE_BLOCKS + 1; i++){
            free_block_numbers[i] = inode_block_count + 2 + data_block_chunk_index * FREE_BLOCKS + i; 
        }

        // free_block_numbers is of size 1008 bytes. write this to the data_block_chunk_index-th block

        write_block(free_block_numbers, inode_block_count + 2 + data_block_chunk_index * FREE_BLOCKS);
    }


    // write remaining blocks

    if (extra_blocks > 0){
        free_block_numbers[0] = extra_blocks;
        lseek(file_handle, (inode_block_count + 2 + data_block_chunk_index * FREE_BLOCKS) * BLOCK_SIZE , SEEK_SET);
        write(file_handle, extra_blocks, 4);

        lseek(file_handle, 4, SEEK_CUR);
        write(file_handle, 0, 4);

        for (i = 2 ; i < extra_blocks; i++){
            lseek(file_handle, 4 , SEEK_CUR);
            write(file_handle, inode_block_count + 2 + data_block_chunk_index * FREE_BLOCKS + i - 1 , 4 );
        }
    }
}


//create root directory and inode.
void create_root_dir()
{
    unsigned int i = 0;
    unsigned short num_bytes;
    unsigned int free_block = get_free_block();

    for (i = 0; i < 28; i++)
        new_directory.filename[i] = 0;

    new_directory.filename[0] = '.'; //root directory's file name is .
    new_directory.filename[1] = '\0';
    new_directory.inode = 1; // root directory's inode number is 1.


    // 1100110000000000
    first_inode.flags = 0b1100110000000000;     // all flags
    first_inode.nlinks = 2;                     // 2 links. one for . and the other for ..
    first_inode.uid = 0;
    first_inode.gid = 0;
    first_inode.size0 = 0;
    first_inode.size1 = 0;
    first_inode.addr[0] = free_block;

    for (i = 1; i < 9; i++)
        first_inode.addr[i] = 0;

    first_inode.actime = time(0);
    first_inode.modtime = time(0);

    // write_inode(first_inode, 0);
    lseek(file_handle, BLOCK_SIZE * 2, SEEK_SET);
    write(file_handle, &first_inode, 64);
    printf("\nFree block %d\n", free_block);
    lseek(file_handle, free_block * BLOCK_SIZE, SEEK_SET);

    // . directory
    
    num_bytes = write(file_handle, &new_directory, DIRECTORY_SIZE);
    if ((num_bytes) < DIRECTORY_SIZE)
        printf("\n Error in writing root directory \n ");

    // .. directory
    
    new_directory.filename[0] = '.';
    new_directory.filename[1] = '.';
    new_directory.filename[2] = '\0';
    num_bytes = write(file_handle, &new_directory, DIRECTORY_SIZE);
    if ((num_bytes) < DIRECTORY_SIZE)
        printf("\n Error in writing root directory\n ");
}

// file system initilization
int initialize_file_system(char *path, unsigned int total_blocks, unsigned int total_inodes_blocks)
{

    unsigned int total_inodes = total_inodes_blocks * BLOCK_SIZE / INODE_SIZE ;
    printf("\nV6 Created by Biswadip, Yangru and Shejo\n");
    char buffer[BLOCK_SIZE];
    int num_bytes;
    super.isize = total_inodes_blocks; //number of inode

    super.fsize = total_blocks;

    unsigned short i = 0;

    if ((file_handle = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
    {
        printf("\n error in opening file [%s]\n", strerror(errno));
        return 1;
    }
    
    //assigning superblock values
    for (i = 0; i < FREE_BLOCKS; i++)
        super.free[i] = 0;

    super.nfree = 0; //number of free block

    super.flock = 'm';
    super.ilock = 'y';
    super.fmod = 'b';
    super.time = time(0); //------------
    lseek(file_handle, BLOCK_SIZE, SEEK_SET);
    if ((num_bytes = write(file_handle, &super, BLOCK_SIZE)) < BLOCK_SIZE)
    {
        printf("\nError in super block writing\n");
        return 0;
    }

    for (i = 0; i < BLOCK_SIZE; i++)
        buffer[i] = 0;

    printf("\nBlocks with Inode count : %d\n", super.isize);
    
    for (i = 0; i < super.isize; i++)
        lseek(file_handle, (i + 2 ) * 1024, SEEK_SET);   
        write(file_handle, buffer, BLOCK_SIZE);         

    // chaining data blocks
    chaining_blocks(total_blocks, super.isize);


    create_root_dir();
    return 1;
}

// get a free data block.&&
unsigned int get_free_block()
{
    unsigned int next_block;
    next_block = super.free[--super.nfree];
    super.free[super.nfree] = 0;

    if (super.nfree == 0)
    {
        int n = 0;
        read_block(block_number_tracker, next_block);
        super.nfree = block_number_tracker[0];
        for (n = 0; n < FREE_BLOCKS; n++)
            super.free[n] = block_number_tracker[n + 1];
    }
    return next_block;
}

//Read integer array from the required block&&
void read_block(unsigned short *dest, unsigned short block_number)
{
    int flag = 0;
    if (block_number > super.isize + super.fsize)
        flag = 1;

    else
    {
        lseek(file_handle, block_number * BLOCK_SIZE, SEEK_SET); //added block size
        read(file_handle, dest, BLOCK_SIZE);
    }
}

//Write integer array to the required block
void write_block(unsigned int *dest, unsigned int bno)
{
    int flag1, flag2;
    int num_bytes;

    if (bno > super.isize + super.fsize)
        flag1 = 1;
    else
    {
        lseek(file_handle, bno * BLOCK_SIZE, 0);
        num_bytes = write(file_handle, dest, BLOCK_SIZE);

        if ((num_bytes) < BLOCK_SIZE)
            flag2 = 1;
    }
    if (flag2 == 1)
    {
        printf("problem with block!\n");
    }
}

//Write to an inode given the inode number	&&
void write_inode(inode_type current_inode, unsigned int inode_index)
{
    int num_bytes;
    lseek(file_handle, 2 * BLOCK_SIZE + inode_index * INODE_SIZE, 0);
    num_bytes = write(file_handle, &current_inode, INODE_SIZE);

    if ((num_bytes) < INODE_SIZE)
        printf("\n Error in inode number : %d\n", inode_index);
}

int main(int argc, char *argv[])
{
    char c;
    int i = 0;
    signal(SIGINT, SIG_IGN);
    char *parser, cmd[512];
    unsigned int bno = 0, inode_block_count = 0;
    char file_path[200];
    char *filename;
    char *num1, *num2;

    printf("\n\nInitialize file system by using\n1. openfs <file_name>\n2.initfs < total blocks> < total inode blocks>\n");
    while (1)
    {
        printf("\nEnter command\n");
        scanf(" %[^\n]s", cmd);
        parser = strtok(cmd, " "); //break string in to a serious tokens using the delimiter " "
        if (strcmp(parser, "initfs") == 0)
        {
            num1 = strtok(NULL, " "); 
            num2 = strtok(NULL, " "); 
            printf("Filesystem: %s", file_path);

            if (access(file_path, X_OK) != -1)
            {

                // open for reading and writing
                if (file_handle = open(file_path, 0x0002) == -1)
                {
                    printf("File system exists, open failed");
                    return 1;
                }
                access(file_path, R_OK | W_OK | X_OK);
                printf("File system already exists.\n");
            }
            else 
            {
                if (!num1 || !num2) //if num1 or num2 equals to zero
                    printf("Enter all arguments in form initfs 560 50.\n");
                else
                {
                    bno = atoi(num1);      //convert a string to an interger(total_blocks)
                    inode_block_count = atoi(num2); //convert a string to an interger(total_inodes)
                    if (initialize_file_system(file_path, bno, inode_block_count))
                    {
                        printf("File System has been Initialized \n");
                    }
                    else
                    {
                        printf("Error: Error in file system initialization\n");
                    }
                }
            }
        }
        else if (strcmp(parser, "openfs") == 0)
        {
            // file_path = strtok(NULL, " "); //read the second breaked tokens
            strcpy(file_path, strtok(NULL, " "));
            printf("%s", file_path);
        }
        else if (strcmp(parser, "q") == 0)
        {
            printf("Exiting the program");
            close(file_handle);
            exit(0);
        }
    }
}
