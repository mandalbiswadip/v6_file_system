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

//GLOBAL CONSTANTS
#define BLOCK_SIZE 1024
#define INODE_SIZE 64
#define inode_alloc 0100000 //Flag value to allocate an inode
#define pfile 000000        //To define file as a plain file
#define lfile 010000        //To define file as a large file
#define directory 040000    //To define file as a Directory
#define max_array 512
#define null_size 1024 // * check where it's getting used
#define FREE_BLOCKS 251

//GLOBAL VARIABLES
int fd;
unsigned short chainarray[max_array];

// super block
typedef struct
{
    unsigned int isize;
    unsigned int fsize;
    unsigned int nfree;
    unsigned int free[FREE_BLOCKS];
    //unsigned short ninode;	  // * need to remove
    //unsigned short inode[100];	// * need to remove
    char flock;
    char ilock;
    char fmod;
    unsigned int time; // * only unsigned int
} fs_super;            //instance of superblock
fs_super super;

//inode

typedef struct
{
    unsigned short flags;
    unsigned short nlinks;
    unsigned int uid;
    unsigned int gid;
    unsigned int size0;
    unsigned int size1;
    unsigned int addr[9];
    unsigned int actime;
    unsigned int modtime;
} fs_inode;
fs_inode initial_inode;

// directory
typedef struct
{
    unsigned int inode;
    char filename[28];
} dirc;
dirc newdir;
dirc dir1; //for duplicates

//nums of inodes, NUM is the number of files of this file system
//struct inode g_usedinode[NUM];

void chaining_blocks(unsigned int total_blocks, unsigned int inode_block_count);
void create_root_dir();
int initializefs(char *path, unsigned int total_blocks, unsigned int total_inodes_blocks);
unsigned int allocatefreedblock();
void read_block(unsigned short *dest, unsigned short bno);
void write_block(unsigned short *dest, unsigned short bno);
void write_inode(fs_inode current_inode, unsigned int inode_index);

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

    lseek(fd, total_blocks * BLOCK_SIZE -1, SEEK_SET);
    write(fd, 0, 1);

    int i = 0;
    for (idx = 0; idx <= FREE_BLOCKS; idx++)
    {
        emptybuffer[idx] = 0;
        free_block_numbers[idx] = 0;
        chainarray[idx] = 0;
        
        if (idx < FREE_BLOCKS){
            super.free[super.nfree] = idx + 2 + inode_block_count + 1;
            ++super.nfree;
        }
    }
    printf("free array first number %d\n", super.free[0]);

    for (data_block_chunk_index = 0 ; data_block_chunk_index < split_blocks; data_block_chunk_index ++){

        printf("data_block_chunk_index: %d\n", data_block_chunk_index);
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
        lseek(fd, (inode_block_count + 2 + data_block_chunk_index * FREE_BLOCKS) * BLOCK_SIZE , SEEK_SET);
        write(fd, extra_blocks, 4);

        lseek(fd, 4, SEEK_CUR);
        write(fd, 0, 4);

        for (i = 2 ; i < extra_blocks; i++){
            lseek(fd, 4 , SEEK_CUR);
            write(fd, inode_block_count + 2 + data_block_chunk_index * FREE_BLOCKS + i - 1 , 4 );
        }
        printf("last byte %d\n", inode_block_count + 2 + data_block_chunk_index * FREE_BLOCKS + i - 1);   
    }
}

//create root directory and inode.
void create_root_dir()
{
    unsigned int i = 0;
    unsigned short num_bytes;
    unsigned int dblock = allocatefreedblock();

    for (i = 0; i < 28; i++)
        newdir.filename[i] = 0;

    newdir.filename[0] = '.'; //root directory's file name is .
    newdir.filename[1] = '\0';
    newdir.inode = 1; // root directory's inode number is 1.

    initial_inode.flags = inode_alloc | directory | 000077;
    initial_inode.nlinks = 1;
    initial_inode.uid = 0;
    initial_inode.gid = 0;
    initial_inode.size0 = 0;
    initial_inode.size1 = 0;
    initial_inode.addr[0] = dblock;

    for (i = 1; i < 9; i++)
        initial_inode.addr[i] = 0;

    initial_inode.actime = 0;
    //initial_inode.modtime[0] = 0;
    initial_inode.modtime = 0;

    // write_inode(initial_inode, 0);
    lseek(fd, BLOCK_SIZE * 2, SEEK_SET);
    write(fd, &initial_inode, 64);
    lseek(fd, dblock * BLOCK_SIZE, SEEK_SET);

    //filling 1st entry with .
    num_bytes = write(fd, &newdir, 16);
    if ((num_bytes) < 16)
        printf("\n Error in writing root directory \n ");

    //filling net entry with ..
    newdir.filename[0] = '.';
    newdir.filename[1] = '.';
    newdir.filename[2] = '\0';
    num_bytes = write(fd, &newdir, 16);
    if ((num_bytes) < 16)
        printf("\n Error in writing root directory\n ");
}

// file system initilization
int initializefs(char *path, unsigned int total_blocks, unsigned int total_inodes_blocks)
{

    unsigned int total_inodes = total_inodes_blocks * BLOCK_SIZE / INODE_SIZE ;
    printf("\nV6 File System by Biswadip, Yangru and Shejo\n");
    char buffer[BLOCK_SIZE];
    int num_bytes;
    super.isize = total_inodes_blocks; //number of inode

    super.fsize = total_blocks;

    unsigned short i = 0;

    if ((fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
    {
        printf("\n open() failed with error [%s]\n", strerror(errno));
        return 1;
    }
    //assigning superblock values
    for (i = 0; i < FREE_BLOCKS; i++)
        super.free[i] = 0;

    super.nfree = 0; //number of free block

    super.flock = 'm';
    super.ilock = 'y';
    super.fmod = 'b';
    super.time = 0000; //------------
    lseek(fd, BLOCK_SIZE, SEEK_SET);
    //#lseek(fd,0,SEEK_SET);
    //#write(fd, &super, 1024);// write 1025 byte from &super to fd

    if ((num_bytes = write(fd, &super, BLOCK_SIZE)) < BLOCK_SIZE)
    {
        printf("\n error in writing the super block\n");
        return 0;
    }

    for (i = 0; i < BLOCK_SIZE; i++)
        buffer[i] = 0;

    printf("Inode count : %d\n", super.isize);
    for (i = 0; i < super.isize; i++)
        lseek(fd, (i + 2 ) * 1024, SEEK_SET);   //reposition read/write file offset i*1024 byte from the start of file
        write(fd, buffer, BLOCK_SIZE);          //write zero to all inode

    // chaining data blocks
    // chainblocks(total_blocks);
    chaining_blocks(total_blocks, super.isize);


    create_root_dir();
    return 1;
}

// get a free data block.&&
unsigned int allocatefreedblock()
{
    unsigned int block;
    block = super.free[--super.nfree];
    super.free[super.nfree] = 0;

    if (super.nfree == 0)
    {
        int n = 0;
        read_block(chainarray, block);
        super.nfree = chainarray[0];
        for (n = 0; n < FREE_BLOCKS; n++)
            super.free[n] = chainarray[n + 1];
    }
    return block;
}

//Read integer array from the required block&&
void read_block(unsigned short *dest, unsigned short bno)
{
    int flag = 0;
    if (bno > super.isize + super.fsize)
        flag = 1;

    else
    {
        lseek(fd, bno * BLOCK_SIZE, SEEK_SET); //added block size
        read(fd, dest, BLOCK_SIZE);
    }
}

//Write integer array to the required block
void write_block(unsigned short *dest, unsigned short bno)
{
    int flag1, flag2;
    int num_bytes;

    if (bno > super.isize + super.fsize)
        flag1 = 1;
    else
    {
        lseek(fd, bno * BLOCK_SIZE, 0);
        num_bytes = write(fd, dest, BLOCK_SIZE);

        if ((num_bytes) < BLOCK_SIZE)
            flag2 = 1;
    }
    if (flag2 == 1)
    {
        printf("problem with block!\n");
    }
}

//Write to an inode given the inode number	&&
void write_inode(fs_inode current_inode, unsigned int inode_index)
{
    int num_bytes;
    lseek(fd, 2 * BLOCK_SIZE + inode_index * INODE_SIZE, 0);
    num_bytes = write(fd, &current_inode, INODE_SIZE);

    if ((num_bytes) < INODE_SIZE)
        printf("\n Error in inode number : %d\n", inode_index);
}

//free data blocks and initialize free array

//Function to update root directory

//Copying from a Small File

// cpout Copying from v-6file System into an external File

//Copying Small file using cpout

//Copying a large file using cpout

// make directory

// display all files

//MAIN
int main(int argc, char *argv[])
{
    char c;
    int i = 0;
    char *tmp = (char *)malloc(sizeof(char) * 200);
    char *cmd1 = (char *)malloc(sizeof(char) * 200);
    signal(SIGINT, SIG_IGN);
    int fs_initcheck = 0;   // bit to check if file system is initialized
    char *parser, cmd[512]; //changed from 256 to 512
    unsigned short n = 0, j, k;
    char buffer1[BLOCK_SIZE];
    unsigned short num_bytes;
    char *name_dir;
    char *cpoutextern;
    char *cpoutsource;
    unsigned int bno = 0, inode_no = 0;
    char *cpinextern;
    char *cpinsource;
    char file_path[200];
    char *filesystem;
    char *filename;
    char *num1, *num2, *num3, *num4;

    printf("\n\nInitialize file system by using initfs <<name of your v6filesystem>> << total blocks>> << total inodes>>\n");
    while (1)
    {
        printf("\nEnter command\n");
        scanf(" %[^\n]s", cmd);
        parser = strtok(cmd, " "); //break string in to a serious tokens using the delimiter " "
        if (strcmp(parser, "initfs") == 0)
        {
            num1 = strtok(NULL, " "); //read the third breaked tokens
            num2 = strtok(NULL, " "); //read the forth breaked tokens
            printf("%s", file_path);

            if (access(file_path, X_OK) != -1)
            {

                // open for reading and writing
                if (fd = open(file_path, 0x0002) == -1)
                {
                    printf("File system exists, open failed");
                    return 1;
                }
                access(file_path, R_OK | W_OK | X_OK);
                printf("File system already exists.\n");
                fs_initcheck = 1;
            }
            else 
            {
                if (!num1 || !num2) //if num1 or num2 equals to zero
                    printf("Enter all arguments in form initfs v6filesystem 5000 400.\n");
                else
                {
                    bno = atoi(num1);      //convert a string to an interger(total_blocks)
                    inode_no = atoi(num2); //convert a string to an interger(total_inodes)
                    if (initializefs(file_path, bno, inode_no))
                    {
                        printf("File System has been Initialized \n");
                        fs_initcheck = 1;
                    }
                    else
                    {
                        printf("Error: File system initialization error.\n");
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
            close(fd);
            exit(0);
        }
    }
}
