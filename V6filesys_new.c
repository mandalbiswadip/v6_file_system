#include "V6filesys_new.h"
#include "utility.h"
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
#include <stdbool.h>


// GLOBAL VARIABLES
int file_handle;
unsigned short block_number_tracker[max_array];
unsigned int block_number_tracker_int[BLOCK_SIZE/sizeof(unsigned int)];
char current_file[MAX_PATH_SIZE] = {'/'};
char slash[1] = {'/'};

dir_type new_directory;

void chaining_blocks(unsigned int total_blocks, unsigned int inode_block_count);
void create_root_dir();
int initialize_file_system(char *path, unsigned int total_blocks, unsigned int total_inodes_blocks);
unsigned int get_free_block();
void read_block(unsigned short *dest, unsigned int bno);
void write_block(unsigned int *dest, unsigned int bno);
void write_inode(inode_type* current_inode, unsigned int inode_index);
bool check_absolute_path(char *path);


void create_file_inode(char *path);   // create inode for file
void create_dir_inode(char *path);    // create inode for dire
char** tokenize_file_path(char *filepath, int *token_count, int max_tokens);
int cpin_handle(char *external_file, char *v6_file);
void read_external_file(char *path);
unsigned int get_inode_no_for_directory(inode_type *current_inode, char *filetoken);

unsigned int fetchNextBlockNumverFilled(inode_type *inode);
bool is_inode_directory(inode_type *inode);
unsigned int make_directory(char *directory_Path);
void read_superblock();



bool check_nth_bit(short a, short n){
    if((short) (a & (1 << n)) != 0){
        return true;
    }
    return false;
}

int min(int x, int y)
{
  return (x < y) ? x : y;
}


// Data blocks chaining procedure
void chaining_blocks(unsigned int total_blocks, unsigned int inode_block_count)
{
    unsigned int free_block_numbers[FREE_BLOCKS + 1];

    unsigned int emptybuffer[FREE_BLOCKS + 1];

    unsigned int linked_blockcount;

    unsigned int data_block_chunk_index;

    unsigned int data_blocks_count = (total_blocks - inode_block_count - 2);

    unsigned int split_blocks =  data_blocks_count / FREE_BLOCKS;           //TODO: check 
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
    super.nfree--;
    next_block = super.free[super.nfree];
    super.free[super.nfree] = 0;

    if (super.nfree == 0)
    {
        printf("Loading new free blocks..\n");
        int n = 0;
        read_block(block_number_tracker_int, next_block);
        super.nfree = block_number_tracker_int[0];
        for (n = 0; n < FREE_BLOCKS; n++){
            super.free[n] = block_number_tracker_int[n + 1];
        }
        super.nfree --;
        next_block = super.free[super.nfree];
        super.free[super.nfree] = 0;
    }
    return next_block;
}

unsigned int get_free_inode_number(superblock_type *super_block){
    // checking for the free available free inode.
    // nlinks is checked if the inode is valid or not
    int inode_number = 1;
    int block_number = 2;
    int nlinks;

    while(block_number < super_block->isize + 2){
        read_block(block_number_tracker, block_number);
        for (int j = 0; j < BLOCK_SIZE / INODE_SIZE ; j++){
            nlinks = block_number_tracker[j * INODE_SIZE / 2 + 1];
            if (nlinks == 0){
                return inode_number;
            }
            inode_number ++;
        }
    }
    return 0;
}


//Read integer array from the required block&&
void read_inode(unsigned short *dest, unsigned short inode_number)
{

    int flag = 0;
    if (inode_number > super.isize * BLOCK_SIZE / INODE_SIZE)
        flag = 1;
    else
    {
        lseek(file_handle, BLOCK_SIZE * 2 + inode_number * INODE_SIZE, SEEK_SET); //added block size
        read(file_handle, dest, INODE_SIZE);
    }
}

void read_superblock()
{
    // read superblock from file system
    char block_data[BLOCK_SIZE];
    
    lseek(file_handle, SUPERBLOCK_NUMBER * BLOCK_SIZE, SEEK_SET); 
    read(file_handle, &block_data, BLOCK_SIZE);
    read_block(block_data, 1);
    bytes_to_superblock(&block_data, &super);   
}

void write_superblock()
{
    // write current superblock state to file system
    
    lseek(file_handle, SUPERBLOCK_NUMBER * BLOCK_SIZE, SEEK_SET); 
    read(file_handle, &super, BLOCK_SIZE - 1);
}


dir_type* read_directory_type(unsigned int address)
{
    dir_type *direc;
    direc = malloc(sizeof(dir_type));
    char direc_bytes[DIRECTORY_SIZE];
    lseek(file_handle, address, SEEK_SET); 
    read(file_handle, &direc_bytes, DIRECTORY_SIZE);
    bytes_to_directory(&direc_bytes, direc);
    return direc;
}


//Read integer array from the required block&&
void read_block(unsigned short *dest, unsigned int block_number)
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


    if (bno > super.isize + super.fsize){
        flag1 = 1;
        int block_written=1;
    }
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
void write_inode(inode_type* current_inode, unsigned int inode_index)
{
    int num_bytes;
    lseek(file_handle, 2 * BLOCK_SIZE + inode_index * INODE_SIZE, 0);
    num_bytes = write(file_handle, current_inode, INODE_SIZE);

    // if ((num_bytes) < INODE_SIZE)
    //     printf("\n Error in inode number : %d\n", inode_index);
}

bool check_absolute_path(char *path){
    if (path[0] == '/'){
        return true;
    }else{
        return false;
    }
}


char** tokenize_file_path(char *filepath, int *token_count, int max_tokens){

    char **file_tokens = malloc(sizeof(char *) * max_tokens);

    int i = 0;
    char * file = strtok(filepath, "/");

    while(file != NULL){
        file_tokens[i] = file;
        i ++;
        file = strtok(NULL, "/");
    }

    *token_count = i;
    return file_tokens;
}


unsigned int get_inode_no_for_directory(inode_type *current_inode, char *filetoken){
    // get inode number in the current inode where filetoken is present. for both file and directory
    unsigned int addresses[INODE_ADDRESSES];
    memcpy( &addresses, current_inode->addr, INODE_ADDRESSES*4);
    unsigned int block_number;
    char block_data[BLOCK_SIZE];
    unsigned int inode_number = 0;
    char inode_filename[28];

    for (int i = 0; i < INODE_ADDRESSES; i++){
        block_number = addresses[i];
        read_block(block_data, block_number);

        for (int j = 0;j < BLOCK_SIZE / DIRECTORY_SIZE ; j++ ){
            memcpy(&inode_number, &block_data[j * DIRECTORY_SIZE ], 4);
            memcpy(&inode_filename, &block_data[j * DIRECTORY_SIZE + 4 ], 28);

            if (inode_number > 0){
                if (strncmp(inode_filename, filetoken, 28) == 0){
                    return inode_number;
                }
            }

        }

    }
    return 0;
}

// get the inode given inode number(starting from 1)
inode_type* get_inode_by_number(unsigned int inode_number){
    inode_type *inode;
    inode = malloc(sizeof(inode_type));
    char inode_data[INODE_SIZE];
    read_inode(&inode_data, inode_number-1);
    bytes_to_inode(&inode_data, inode);
    return inode;
}

bool is_inode_directory(inode_type *inode){
    bool b = check_nth_bit(inode->flags, 14);
    bool c = check_nth_bit(inode->flags, 13);
    return b && !c;
}


void fill_file_inode(inode_type *file_inode){
    // small file
    file_inode->flags = 0b1000110000000000;
    file_inode->nlinks = 1;
    file_inode->uid = 1;                            
    file_inode->gid = 1;
    file_inode->size0 = 0;
    file_inode->size1 = 0;
    file_inode->actime = time(0);
    file_inode->modtime = time(0);
}

void fill_directory_inode(inode_type *dir_inode){
    // b, c is 1,0
    dir_inode->flags = 0b1100110000000000;
    dir_inode->nlinks = 2;
    dir_inode->uid = 1;                            
    dir_inode->gid = 1;
    dir_inode->size0 = 0;
    dir_inode->size1 = 0;
    dir_inode->actime = time(0);
    dir_inode->modtime = time(0);
}

// initializing a inode 
static void init_Inode(inode_type *inode) {
    inode->flags = 0;
    inode->nlinks = 0;
    inode->uid = 0;
    inode->gid = 0;
    inode->size0 = 0;
    inode->size1 = 0;
    for (size_t i = 0; i < 9; i++) {
        inode->addr[i] = 0;
    }
    inode->actime = 0;
    inode->modtime = 0;
}


int add_directory_to_inode(inode_type *inode, char *filename, unsigned int inode_number){

    if(is_inode_directory(inode) == false){
        printf("\nFailed to save directory type for %s\n", filename);
        return -1;
    }

    dir_type new_directory;
    new_directory.inode = inode_number;
    strncpy(new_directory.filename, filename,  MAX_FILE_TOKENS);

    dir_type *current_dir;

    unsigned int address_block;
    unsigned short dir_index;
    unsigned short i;

    for(i=0 ; i < 9 ; i++){
        address_block = inode->addr[i];

        if(address_block == 0){
            address_block = get_free_block();
            lseek(file_handle, address_block * BLOCK_SIZE, SEEK_SET);
            write(file_handle, &new_directory, DIRECTORY_SIZE);
            inode->addr[i] = address_block;
            return 0;
        }

        for (dir_index = 0; dir_index < BLOCK_SIZE / DIRECTORY_SIZE; dir_index++){
            current_dir = read_directory_type(address_block * BLOCK_SIZE + dir_index * DIRECTORY_SIZE);

            if (current_dir->inode == 0){
                // store directory
                lseek(file_handle, address_block * BLOCK_SIZE + dir_index * DIRECTORY_SIZE, SEEK_SET);
                write(file_handle, &new_directory, DIRECTORY_SIZE);
                return 0;
            }
        }
    }
    return 0;
}

unsigned int get_inode_file_size(inode_type *inode){
    // TODO to include size1 for later file for later parts of the assignments
    return inode->size0;
}

char* normalize_file(char *v6_file){
    
    bool absolute = check_absolute_path(v6_file);
    char *new_path;
    new_path = malloc(sizeof(current_file));
    if (absolute == false){
        
        strcpy(new_path, current_file);
        strcat(new_path, v6_file);
    }
    else{
        strcat(new_path, v6_file);
    }
    return new_path;
}

int cpin_handle(char *external_file, char *v6_file){
    if (external_file == NULL || v6_file == NULL){
        printf("\nINVALID INPUT! Follow this format\ncpin externalfile v6-file\n");
        return -1;
    }

    int token_count = 0;
    
    printf("External file: %s\n", external_file);
    printf("File system path for saving: %s\n", v6_file);
    
    char **file_tokens = tokenize_file_path(v6_file, &token_count, 40);

    printf("token count %d\n",token_count);

    inode_type *curr_inode = NULL;
    inode_type *previous_inode = get_inode_by_number(1);
    unsigned int current_inode_no = 0;
    unsigned int previous_inode_no = 1;

    
    // iterate through the file path
    // leaving the last token out as inode_no = 0 means that the file exists, 
    // but for directory it means that directory doesn't exist
    for (int i = 0 ; i < token_count - 1; i++){
        current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[i]);

        if (current_inode_no == 0){
            // create the folder if it doesn't exists
            current_inode_no = get_free_inode_number(&super);
            printf("%s doesn't exists. Creating the folder at inode %d\n", file_tokens[i], current_inode_no);
            curr_inode =  get_inode_by_number(current_inode_no);
            fill_directory_inode(curr_inode);


            // add . and .. to the current directory
            add_directory_to_inode(curr_inode, ".", current_inode_no);
            add_directory_to_inode(curr_inode, "..", previous_inode_no);
            write_inode(curr_inode, current_inode_no - 1);

            // link with parent
            printf("adding direcctory in parent for %s\n", file_tokens[i]);
            add_directory_to_inode(previous_inode, file_tokens[i], current_inode_no);


            write_inode(previous_inode, previous_inode_no - 1);

        }else{
            printf("%s already exists\n", file_tokens[i]);
            curr_inode = get_inode_by_number(current_inode_no);
        }

        free(previous_inode);
        previous_inode = curr_inode;
        previous_inode_no = current_inode_no;
    }


    // for the last token or file.
    current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[token_count - 1]);

    if(current_inode_no != 0){
        printf("File already exists!!");
    }else{
        current_inode_no =  get_free_inode_number(&super);
        if (current_inode_no == 0){
            printf("Memory for file system exceeds!!");
            return 0;
        }
        curr_inode = get_inode_by_number(current_inode_no);

        // link with parent
        add_directory_to_inode(previous_inode, file_tokens[token_count-1], current_inode_no);
        write_inode(previous_inode, previous_inode_no - 1);
        printf("File %s doesn't exists. Creating the file at inode %d\n", file_tokens[token_count -1], current_inode_no);

        fill_file_inode(curr_inode);       
        
        FILE *f = fopen(external_file, "rb");
        char data_store[BLOCK_SIZE];
        unsigned int free_block;
        int address_i = 0;
        unsigned int total_size = 0;


        // for small sizes
        while(feof(f)==0 && address_i < INODE_ADDRESSES){
            free_block = get_free_block();
            if (free_block == 0){
                printf("No free block\n");
                return 0;
            }
            size_t num_bytes = fread(data_store, 1, BLOCK_SIZE, f);
            total_size += num_bytes;
            write_block(data_store, free_block);
            curr_inode->addr[address_i] = free_block;
            address_i ++;
        }
        curr_inode->size0 = total_size;
        printf("Size of total file %d\n", total_size);

        write_inode(curr_inode, current_inode_no - 1);
        
    }
    return 0;
}


int cout_handle(char *external_file, char *v6_file){
    if (external_file == NULL || v6_file == NULL){
        printf("\nINVALID INPUT! Follow this format\ncpout v6-file externalfile\n");
        return -1;
    }
    int token_count = 0;
    
    printf("External file: %s\n", external_file);
    printf("V6 File: %s\n", v6_file);
    bool absolute = check_absolute_path(v6_file);

    char **file_tokens = tokenize_file_path(v6_file, &token_count, 40);

    printf("token count %d\n",token_count);

    inode_type *curr_inode = NULL;
    inode_type *previous_inode = get_inode_by_number(1);
    unsigned int current_inode_no = 0;
    unsigned int previous_inode_no = 1;
    printf("token count %d\n",token_count);
    
    // iterate through the file path
    // leaving the last token out as inode_no = 0 means that the file exists, 
    // but for directory it means that directory doesn't exist
    for (int i = 0 ; i < token_count - 1; i++){
        current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[i]);

        if (current_inode_no == 0){
            
            printf("Directory %s doesn't exists!!\n", file_tokens[i]);
            return -1;
        }else{
            printf("%s exists\n", file_tokens[i]);
            curr_inode = get_inode_by_number(current_inode_no);
        }

        free(previous_inode);
        previous_inode = curr_inode;
        previous_inode_no = current_inode_no;
    }


    // for the last token or file.
    current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[token_count - 1]);
    printf("File inode %d\n", current_inode_no);
    if(current_inode_no != 0){
        printf("File exists. copying it!!\n");
        FILE *f = fopen(external_file, "wb");
        curr_inode = get_inode_by_number(current_inode_no);
        unsigned int file_size = get_inode_file_size(curr_inode);
        printf("File size for %s is %d\n", file_tokens[token_count - 1], file_size);

        unsigned int address_block;
        unsigned short dir_index;
        unsigned short i;
        char data[BLOCK_SIZE];

        for(i=0 ; i < INODE_ADDRESSES ; i++){
            address_block = curr_inode->addr[i];

            if(address_block == 0){
                printf("Ending copying!!\n");
                return 0;
            }
            printf("Copying block %d\n", address_block);
            read_block(data, address_block);

            if (file_size > BLOCK_SIZE){
                fwrite(data, 1, BLOCK_SIZE, f );
                file_size -= BLOCK_SIZE;
            }else{
                fwrite(data, 1, file_size, f);
                fclose(f);
                file_size = 0;
            }
        }
    }else{
        printf("File %s doesn't exist!!", file_tokens[token_count-1]);    
        return -1;
    }
    return 0;
}


void makedir(char *directory_Path) {
    printf("Creating Directory : %s\n", directory_Path);
    int inode_count;

    inode_count = make_directory(directory_Path);//  FILE_TYPE_DIRECTORY deleted

    if (inode_count == 0) {
        printf("Error creating directory!!");
    }
}

unsigned int fetchNextBlockNumverFilled(inode_type *inode){
    return 0;
}


unsigned int check_dir(char *directory_Path){
    int token_count = 0;
    
    char **file_tokens = tokenize_file_path(directory_Path, &token_count, 40);


    inode_type *curr_inode = NULL;
    inode_type *previous_inode = get_inode_by_number(1);
    unsigned int current_inode_no = 0;
    unsigned int previous_inode_no = 1;

    
    // iterate through the file path
    // leaving the last token out as inode_no = 0 means that the file exists, 
    // but for directory it means that directory doesn't exist
    for (int i = 0 ; i < token_count; i++){
        current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[i]);

        if (current_inode_no == 0){   
            printf("ERROR. %s folder doesn't exists\n", file_tokens[i]);
            return 1;
        }else{
            printf("Folder %s exists\n", file_tokens[i]);
            curr_inode = get_inode_by_number(current_inode_no);
        }

        free(previous_inode);
        previous_inode = curr_inode;
        previous_inode_no = current_inode_no;
    }
    return 0;
}

unsigned int make_directory(char *directory_Path){
    int token_count = 0;    
    char **file_tokens = tokenize_file_path(directory_Path, &token_count, 40);

    inode_type *curr_inode = NULL;
    inode_type *previous_inode = get_inode_by_number(1);
    unsigned int current_inode_no = 0;
    unsigned int previous_inode_no = 1;

    
    // iterate through the file path
    // leaving the last token out as inode_no = 0 means that the file exists, 
    // but for directory it means that directory doesn't exist
    for (int i = 0 ; i < token_count; i++){
        current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[i]);

        if (current_inode_no == 0){
            
            // create the folder if it doesn't exists
            current_inode_no = get_free_inode_number(&super);
            printf("%s doesn't exists. Creating the folder at inode %d\n", file_tokens[i], current_inode_no);
            curr_inode =  get_inode_by_number(current_inode_no);
            fill_directory_inode(curr_inode);


            // add . and .. to the current directory
            add_directory_to_inode(curr_inode, ".", current_inode_no);
            add_directory_to_inode(curr_inode, "..", previous_inode_no);
            write_inode(curr_inode, current_inode_no - 1);

            // link with parent
            printf("adding direcctory in parent for %s\n", file_tokens[i]);
            add_directory_to_inode(previous_inode, file_tokens[i], current_inode_no);


            write_inode(previous_inode, previous_inode_no - 1);

        }else{
            printf("%s already exists\n", file_tokens[i]);
            curr_inode = get_inode_by_number(current_inode_no);
        }

        free(previous_inode);
        previous_inode = curr_inode;
        previous_inode_no = current_inode_no;
    }
    return curr_inode;
}


/*
* remove a dir
*
*/


//save a inode 
int save_Inode(superblock_type *sb, unsigned int inode_count, inode_type *inode) {
    unsigned int inode_block_count;
    unsigned int offset_in_block;
    int block_data[BLOCK_SIZE];

    if (inode_count > sb->isize * 16) {
        return -1;
    }

    inode_block_count = (inode_count - 1) / 16 + 2;
    offset_in_block = ((inode_count - 1) % 16) * INODE_SIZE;

    read_block(&block_data, inode_block_count);  // doubt
    inode_to_bytes(inode, &block_data[offset_in_block]);
    write_block(&block_data, inode_block_count); //doubt

    return 0;
}

/*
 * Frees the given block number. Updates the superblock accordingly.
 */
int free_block(superblock_type *sb, unsigned int block_count) {
    int bytes_written;
    unsigned int *block_data;
    int block_written;

    if (block_count < 2 || block_count >= sb->fsize) {
        return -1;
    }

    if (sb->nfree == 251) {
        // Allocate one 1024 byte chunk to buffer write data
        block_data = malloc(sizeof(unsigned int) * 512);

        block_data[0] = sb->nfree;
        memcpy(&block_data[1], sb->free, sb->nfree * 2);

        write_block(&block_data, block_count); //doubt
        if (block_written != 0) {
            free(block_data);
            return block_written;
        }

        sb->nfree = 0;

        free(block_data);
    }

    sb->free[sb->nfree] = block_count;
    sb->nfree++;

    return 0;
}

//free an inode
int freeInode(superblock_type *sb, int inode_no){
    if(inode_no < 1 || inode_no > sb -> isize * BLOCK_SIZE / INODE_SIZE){
        printf("Invalid inode no %d. Can't delete\n", inode_no);
        return -1;
    }

    inode_type* empty_inode;

    write_inode(empty_inode, inode_no);

}


//delete a directory entry
int delete_directory_entry(inode_type *inode, unsigned int inode_to_delete) {
    char inode_FileName[MAX_FILE_TOKENS] = { 0 };
    
    // define empty directory
    dir_type new_directory;
    new_directory.inode = 0;
    strncpy(new_directory.filename, inode_FileName,  MAX_FILE_TOKENS);

    dir_type *current_dir;

    unsigned int address_block;
    unsigned short dir_index;
    unsigned short i;

    for(i=0 ; i < 9 ; i++){
        address_block = inode->addr[i];

        // if(address_block == 0){
        //     printf("File inode entry couldn't be found\n");
        //     return 0;
        // }


        // TODO: if the first directory in the block, add the block to free block list
        for (dir_index = 0; dir_index < BLOCK_SIZE / DIRECTORY_SIZE; dir_index++){
            current_dir = read_directory_type(address_block * BLOCK_SIZE + dir_index * DIRECTORY_SIZE);

            if (current_dir->inode == inode_to_delete){
                // store directory
                lseek(file_handle, address_block * BLOCK_SIZE + dir_index * DIRECTORY_SIZE, SEEK_SET);
                write(file_handle, &new_directory, DIRECTORY_SIZE);
                return 0;
            }
        }
    }
}



//  remove a file 
int delete_a_file(superblock_type *sb, char *file_path){
    printf("Deleting file: %s\n", file_path );
    int token_count = 0;

    inode_type *curr_inode = NULL;
    inode_type *previous_inode = get_inode_by_number(1);

    unsigned int current_inode_no = 0;
    unsigned int previous_inode_no = 1;

    char **file_tokens = tokenize_file_path(file_path, &token_count, 40); 

    for(int i=0; i<token_count-1; i++){
        current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[i]);
        
        if(current_inode_no==0){
            if(curr_inode != NULL){
                free(curr_inode);
            }
            free(previous_inode);
            printf("%s directory doesn't exists\n", file_tokens[i]);
            return -1;
        }

        curr_inode = get_inode_by_number(current_inode_no); //need to check 

        free(previous_inode);
        previous_inode = curr_inode;
        previous_inode_no = current_inode_no;
    }

    current_inode_no = get_inode_no_for_directory(previous_inode, file_tokens[token_count-1]);


    if(current_inode_no==0){
        printf("File %s can't be deleted as it doesn't exist\n", file_tokens[token_count-1]);
        return -1; //NO_FILE_EXIST;
    }else{
        curr_inode = get_inode_by_number(current_inode_no);
        if (is_inode_directory(curr_inode) == true){
            printf("Given input is a directory. rm only works for files\n");
        }
        delete_directory_entry(previous_inode, current_inode_no);
        freeInode(sb, current_inode_no - 1);
        return 0;
    }
}

int handle_command(char *argv[])
{
    char c;
    int i = 0;
    signal(SIGINT, SIG_IGN);
    char *parser, cmd[512];
    unsigned int bno = 0, inode_block_count = 0;
    char file_path[MAX_PATH_SIZE];
    char *filename;
    char *external_file, * v6_file;
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
            printf("Filesystem: %s\n", file_path);

            if (access(file_path, F_OK) != -1)
            {
                if ((file_handle = open(file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
                {
                    printf("\n error in opening file [%s]\n", strerror(errno));
                    return 1;
                }
                access(file_path, R_OK | W_OK | X_OK);
                printf("File system already exists.\n");
                read_superblock();
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
            printf("%s\n", file_path);
        }
        else if (strcmp(parser, "cpin") == 0)
        {
            external_file = strtok(NULL, " "); 
            v6_file = strtok(NULL, " ");
            strcpy(v6_file, normalize_file(v6_file));
            cpin_handle(external_file, v6_file);
        }

        else if (strcmp(parser, "cpout") == 0)
        {
            char *normalized_file;
            normalized_file = malloc(MAX_FILE_TOKENS);
            v6_file = strtok(NULL, " "); 
            external_file = strtok(NULL, " "); 
            strcpy( normalized_file, normalize_file(v6_file));
            cout_handle(external_file, normalized_file);
        }
        else if (strcmp(parser, "mkdir") == 0)
        {
            v6_file = strtok(NULL, " "); 
            strcpy(v6_file, normalize_file(v6_file));
            makedir(v6_file);
        }

        else if (strcmp(parser, "rm") == 0)
        {
            v6_file = strtok(NULL, " "); 
            strcpy(v6_file, normalize_file(v6_file));
            delete_a_file(&super, v6_file);
        }
        else if (strcmp(parser, "cd") == 0)
        {
            char *normalized_file;
            normalized_file = malloc(MAX_FILE_TOKENS);
            
            v6_file = strtok(NULL, " "); 
            // v6_file = normalize_file(v6_file);
            strcpy(v6_file, normalize_file(v6_file));
            strcpy(normalized_file, v6_file);

            unsigned int direc_flag = check_dir(v6_file);

            if (direc_flag != 1){
                strcpy(current_file, normalized_file);
                if (current_file[strlen(current_file)-1] != '/'){
                    strcat(current_file, slash);
                }
                printf("Current directory now at: %s\n", current_file);
            }else{
                printf("Directory doesn't exists!");
            }
            // free(v6_file); 
        }
        else if (strcmp(parser, "q") == 0)
        {
            printf("Saving changes and Exiting the program");
            write_superblock();
            close(file_handle);
            exit(0);
        }
        else{
            printf("INVALID COMMDAND!!");
        }
        printf("\n\n");
    }
    return 0;
}

int main(int argc, char *argv[]){
    handle_command(argv);
    return 0;
}

        


