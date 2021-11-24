#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>

#define FREE_BLOCK_SIZE 152  
#define BLOCK_SIZE 1024    
#define ADDRESSES_SIZE 11
#define INPUT_ARRAY 256
#define MAX_FILE_TOKENS 14

// Superblock Structure
typedef struct {
  unsigned short isize;
  unsigned short fsize;
  unsigned short ninode;
  unsigned short nfree;
  unsigned int free[FREE_BLOCK_SIZE];
  unsigned short inode[200];
  char flock;
  char ilock;
  unsigned short fmod;
  unsigned short time[2];
} superblock_type;

// inode struct
typedef struct {
  unsigned short flags;
  unsigned short nlinks;
  unsigned short uid;
  unsigned short gid;
  unsigned int size;
  unsigned int addr[ADDRESSES_SIZE];
  unsigned short actime[2];
  unsigned short modtime[2];
} inode_type; 

// Directory structure
typedef struct {
  unsigned short inode;
  unsigned char filename[MAX_FILE_TOKENS];
} dir_type;

const unsigned short inode_alloc = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short plain_file_flag = 000000;
const unsigned short dir_access_rights = 000777; // User, Group, & World have all access privileges 
const unsigned short INODE_SIZE = 64; // inode has been doubled

// supporting global variables
superblock_type superBlock;
inode_type dirInode;
inode_type fileInode;
dir_type root;

unsigned short dir_inode_entry;

char v6FileName[MAX_FILE_TOKENS];        // current working v6 file system

int f_des ;		//file descriptor

// initialization functions
int init_help(char *parameters);
int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);
void create_root();

// copy in and copy out
int copyIn(char *parameters);
int copyOut(char *parameters);

char currDirPath[80];           // current working path in v6 file system

// mkdir functions
unsigned int createDir(char *parameters);
unsigned short addDir(unsigned short num_dir_inode_addr, unsigned short dir_pos_in_data_blcok, char *newDirName);

// functions related to remove 
unsigned int removeFile(char *rmFileName);
void rm_empty_dir();
void rmPlainFile();

//cd and ls 
unsigned int change_dir(char *parameters);
void show_fl_dir();

// functions that support main functions
void edit_fl_name(char *parameters);
void showSuper();
void readSuper();
void readDirInode(unsigned short entry);
void readFileInode(unsigned short entry);
unsigned short getFreeInode();
void add_to_inode_list(unsigned short inode_entry);
unsigned int getFreeBlock();
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer);

/**********************************************************************************************************/
/* main execution function */
int main() {
 
  char input[INPUT_ARRAY];
  char *splitter;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  printf("Size of super block = %d , size of i-node = %d\n",sizeof(superBlock),sizeof(dirInode));
  printf("");
  while(1) {
    printf("\nEnter command or 'help' for command instruction:\n");
    printf(currDirPath);
    printf("\n");
    printf("-$ ");

    scanf(" %[^\n]s", input);
    splitter = strtok(input," ");
    
    if(strcmp(splitter, "initfs") == 0){
    
        init_help(splitter);
        splitter = NULL;
                       
    } else if (strcmp(splitter, "v6Name") == 0){
        splitter = strtok(NULL, " ");
        edit_fl_name(splitter);
        splitter = NULL;

    } else if (strcmp(splitter, "cpin") == 0) {

        if (v6FileName == NULL || v6FileName[0] == '\0'){
          printf("Choose a v6-file system by entering filename of it using command: v6Name <filename>\n");
          continue;
        }

        if (copyIn(splitter)){
          printf("\nSuccessfully copy in!\n");
        }
        
        splitter = NULL;

    } else if (strcmp(splitter, "cpout") == 0) {

        if (v6FileName == NULL || v6FileName[0] == '\0'){
          printf("Choose a v6-file system by entering filename of it using command: v6Name <filename>\n");
          continue;
        }

        if (copyOut(splitter)){
          printf("\nSuccessfully copy out!\n");
        }
        
        splitter = NULL;

    } else if (strcmp(splitter, "rm") == 0){
      
      if (v6FileName == NULL || v6FileName[0] == '\0'){
        printf("Choose a v6-file system by entering filename of it using command: v6Name <filename>\n");
        continue;
      }

      splitter = strtok(NULL, " ");
      if (removeFile(splitter))
        printf("\nFile successfully removed!\n");

      splitter = NULL;

    } else if (strcmp(splitter, "mkdir") == 0){
      
      if (v6FileName == NULL || v6FileName[0] == '\0'){
        printf("Choose a v6-file system by entering filename of it using command: v6Name <filename>\n");
        continue;
      }

      splitter = strtok(NULL, " ");
      if(createDir(splitter))
        printf("\nDirectory successfully created!\n");
      splitter = NULL;

    } else if (strcmp(splitter, "cd") == 0){
      
      if (v6FileName == NULL || v6FileName[0] == '\0'){
        printf("Choose a v6-file system by entering filename of it using command: v6Name <filename>\n");
        continue;
      }

      splitter = strtok(NULL, " ");
      if(!change_dir(splitter))
        printf("Certain file directory is not found!");

      splitter = NULL;

    } else if (strcmp(splitter, "ls") == 0){

      if (v6FileName == NULL || v6FileName[0] == '\0'){
        printf("Choose a v6-file system by entering filename of it using command: v6Name <filename>\n");
        continue;
      }

      show_fl_dir();

      splitter = NULL;

    } else if (strcmp(splitter, "help") == 0){

        printf("\nInitalization v6 file system: initfs <filename> <# of block> <# of i-nodes>\n");
        printf("\nChoose the v6 file system: v6Name <filename>\n");
        printf("\nCopy external file into v6 file system: cpin <externalfile> <v6-file>\n");
        printf("\nCopy file in v6 file system out to external file: cpout <v6-file> <externalfile>\n");
        printf("\nRemove file: rm <v6-file>\n");
        printf("\nCreate new directory: mkdir <v6dir>\n");
        printf("\nChange current directory: cd <v6dir>\n");
        printf("\nDisplay files in current directory: ls\n");
        printf("\nExit the program: q\n");
        splitter = NULL;

    } else if (strcmp(splitter, "q") == 0) {
   
       lseek(f_des, BLOCK_SIZE, SEEK_SET);
       write(f_des, &superBlock, BLOCK_SIZE);
       return 0;
    }
  }
}
/**********************************************************************************************************/

void edit_fl_name(char *parameters){
  /* 
  * Set up the name of the v6 file system
  * Read the superBlock info in the corresponding v6 file system
  */

  int i,j;
  
  if (currDirPath != NULL)
    memset(currDirPath, 0, strlen(currDirPath));

  for (i = 0; i < MAX_FILE_TOKENS; i++){
    if (parameters[i] == '\0'){
      j = i;
      currDirPath[j] = ':';
      currDirPath[++j] = '/';

      for (j = i; j < MAX_FILE_TOKENS; j++)
        v6FileName[j] = '\0';
      break;
    }
    v6FileName[i] = parameters[i];
    currDirPath[i] = parameters[i];
  }

  if((f_des = open(v6FileName,O_RDWR,0700))== -1) {
    printf("\n No such file exist or open() failed with the following error [%s]\n",strerror(errno));
    v6FileName[0] = '\0';
    return;
  }

  printf("The current v6 file path (name) is %s\n",v6FileName);
  
  readSuper();
  showSuper();
  readDirInode(1);
}

unsigned int createDir(char *parameters){
  /*
  * Recursive function until parameters == NULL
  * check the existence of each file and get the necessary parameters
  * update dirInode after each recursive call
  * go to addDir(...) for each directory creation
  */

  // dirInode is corresponding to currDirPath
  
  char *dir_name;
  dir_name = strtok_r(NULL, "/", &parameters);
  if(strlen(dir_name) > MAX_FILE_TOKENS){
    printf("Directory names cannot exceed 14 characters");
    return 0;
  }
  if (parameters != NULL && parameters[0]=='\0')
    parameters = NULL;
  /*printf("commands: %s\n",parameters);
  printf("Executing directory name: %s\n",dir_name);
  printf("Left commands: %s\n\n",parameters);*/
  
  // go to current directory
  char existFilename[MAX_FILE_TOKENS];
  unsigned short countDir = 2;          // used to jump to the next dirInode.addr[] data block
  unsigned short num_dir_inode_addr = 0;

  lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE+countDir*16, 0);
  read(f_des, &dir_inode_entry, 2);
  read(f_des, existFilename, MAX_FILE_TOKENS);

  while(existFilename[0] != '\0'){
    if (strcmp(existFilename, dir_name) == 0){
      unsigned short flag;
      lseek(f_des, 2*BLOCK_SIZE+(dir_inode_entry-1)*INODE_SIZE,0);
      read(f_des,&flag,2);
      if(flag >= (inode_alloc | dir_flag)){
        printf("\nfilename of the v6-directory is found!\n");
        break;
      }
      else {
        printf("\nfilename is v6-file, but not directory type!Keep finding......\n");
      }
    }

    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + (++countDir)*16, SEEK_SET);
    
    if (countDir == BLOCK_SIZE / 16){
      // only when number of data blocks > 1
      countDir = 0;
      lseek(f_des, dirInode.addr[++num_dir_inode_addr]*BLOCK_SIZE, SEEK_SET);
    }

    read(f_des, &dir_inode_entry, 2);
    read(f_des, existFilename, MAX_FILE_TOKENS);
  }

  if (existFilename[0] != '\0')
    printf("\nFile directory %s already exist\n", dir_name);
  else{ 
    dir_inode_entry = addDir(num_dir_inode_addr, countDir, dir_name);
    //printf("\nNew file directory %s is created, inode-entry: %hu\n", dir_name, dir_inode_entry);
  }
  readDirInode(dir_inode_entry);    // load the directory inode information for the next createDir call
  
  // Add on to the currDirPath
  unsigned short L1 = strlen(currDirPath), L2 = strlen(dir_name),i;
  for (i = 0; i < L2; i++){
    currDirPath[i+L1] = dir_name[i];
  }

  currDirPath[L1+L2] = '/';

  lseek(f_des, BLOCK_SIZE, SEEK_SET);
  write(f_des, &superBlock, BLOCK_SIZE);
  
  if (parameters != NULL)
    return createDir(parameters);

  return 1;
}

unsigned short addDir(unsigned short num_dir_inode_addr, unsigned short dir_pos_in_data_blcok, char *newDirName){
  /* 
  * Create new directory: 
  * Initialize its data block with two directory entries (16 bytes) and its i-node in i-node blocks
  * Add a new directory entry (16 bytes) in its parent data block
  */
  unsigned int new_data_block;
  int i;
  
  dir_type new_dir;
  new_dir.inode = getFreeInode();   // get available inode number.
  if (new_dir.inode == -1)
    return 0;

  // wrtie on the parent data block
  for (i = 0; i < MAX_FILE_TOKENS; i++){
    if (newDirName[i] == '\0'){
      new_dir.filename[i] = '\0';
      break;
    }
    new_dir.filename[i] = newDirName[i];
  }
  
  unsigned int par_Inode;
  lseek(f_des, dirInode.addr[0]*BLOCK_SIZE, 0);
  read(f_des, &par_Inode, 2);

  if (dir_pos_in_data_blcok == BLOCK_SIZE / 16){
    // the current root data block happens to be full, get the new free block for directlory
    // num_dir_inode_addr: current number of full data blocks that belongs to the directory
    // update Inode of the directory
    
    new_data_block = getFreeBlock();
    lseek(f_des, 2 * BLOCK_SIZE + par_Inode*INODE_SIZE + 12 + 4*num_dir_inode_addr, 0);
    write(f_des, &new_data_block, 4);
    dirInode.addr[num_dir_inode_addr+1] = new_data_block;

    lseek(f_des, new_data_block*BLOCK_SIZE, 0);
    write(f_des, &new_dir, 16);
  } else {
    // the current root data block is not full
    // num_dir_inode_addr: current number of full data blocks that belongs to the directory
    // locate dirInode.addr[num_dir_inode_addr]
    // update data block of the directory

    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE+dir_pos_in_data_blcok*16, 0);
    write(f_des, &new_dir, 16);
  }

  new_data_block = getFreeBlock();

  dirInode.flags = inode_alloc | dir_flag | dir_access_rights;   		// flag for directory type
  dirInode.nlinks = 0; 
  dirInode.uid = 0;
  dirInode.gid = 0;
  dirInode.size = INODE_SIZE;
  dirInode.addr[0] = new_data_block;
  
  for( i = 1; i < ADDRESSES_SIZE; i++ ) {
    dirInode.addr[i] = 0;
  }
  
  dirInode.actime[0] = 0;
  dirInode.modtime[0] = 0;
  dirInode.modtime[1] = 0;
  
  // wrtie the i-node entry
  lseek(f_des, 2 * BLOCK_SIZE+(new_dir.inode-1)*INODE_SIZE, 0);
  write(f_des, &dirInode, INODE_SIZE);

  // wrtie on the self data block
  new_dir.filename[0] = '.';
  new_dir.filename[1] = '\0';

  lseek(f_des, new_data_block*BLOCK_SIZE, 0);
  write(f_des, &new_dir, 16);
  
  dir_type par_dir;
  par_dir.inode = par_Inode;
  par_dir.filename[0] = '.';
  par_dir.filename[1] = '.';
  par_dir.filename[2] = '\0';
  
  write(f_des, &par_dir, 16);

  return new_dir.inode;
}

unsigned int removeFile(char *rmFileName){
  /*
  * remove a file (filepath) starts from current directory path
  * if a directory contains no plain files, it can be deleted, otherwise not.
  */  

  if(strlen(rmFileName) > MAX_FILE_TOKENS){
    printf("v6 filename cannot exceed 14 characters");
    return 0;
  }
  
  /*** go to the current directory path to check if v6 file exist, if file not exist, abort ***/
  char existFilename[MAX_FILE_TOKENS];
  unsigned short countDir = 2;          // start from the 3rd file
  unsigned short num_dir_inode_addr = 0;
  unsigned short rm_file_inode_entry;

  lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE+32, 0);
  read(f_des, &rm_file_inode_entry, 2);
  read(f_des, existFilename, MAX_FILE_TOKENS);

  while(existFilename[0] != '\0'){
    if (strcmp(existFilename, rmFileName) == 0){
      unsigned short flag;
      lseek(f_des, 2*BLOCK_SIZE+(rm_file_inode_entry-1)*INODE_SIZE,0);
      read(f_des,&flag,2);
      
      // dir and plain type files might have the same name
      if(flag >= (inode_alloc | dir_flag)){
        printf("\nDeleted file type is a directory!\n");
        unsigned short file_entry;
        lseek(f_des, fileInode.addr[0]*BLOCK_SIZE + 16*2,SEEK_SET);
        read(f_des, &file_entry, 2);
        if (file_entry != 0){
          printf("But the directory is not empty. Can't be deleted! Keep finding...\n");
        } else {
          readFileInode(rm_file_inode_entry);
          rm_empty_dir();
          break;
        }
      } else {
        printf("\nDeleted file type is a plain file!\n");
        readFileInode(rm_file_inode_entry);
        rmPlainFile();
        break;
      }
    }
    
    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + (++countDir)*16, SEEK_SET);

    if (countDir == BLOCK_SIZE / 16){
      // only when number of data blocks > 1
      countDir = 0;
      lseek(f_des, dirInode.addr[++num_dir_inode_addr]*BLOCK_SIZE, SEEK_SET);
    }

    read(f_des, &rm_file_inode_entry, 2);
    read(f_des, existFilename, MAX_FILE_TOKENS);
  }

  if (existFilename[0] == '\0'){
    printf("\nCorresponding file is not found!\n");
    return 0;
  }

  // free the inode entry
  add_to_inode_list(rm_file_inode_entry);

  // delete the directory type (16 bytes) in parent directory and move one directory ahead after it
  unsigned int i,buffer[4];
  for (i = 0; i < 4; i++)
    buffer[i] = 0;
  lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + countDir*16, SEEK_SET);
  write(f_des, buffer, 16);

  dir_type tmp_dir;
  read(f_des, &tmp_dir.inode, 2);
  read(f_des, tmp_dir.filename, MAX_FILE_TOKENS);
  while (tmp_dir.inode != 0){
    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + (countDir++)*16, SEEK_SET);
    write(f_des, &tmp_dir, 16);
    write(f_des, buffer, 16);
    read(f_des, &tmp_dir.inode, 2);
    read(f_des, tmp_dir.filename, MAX_FILE_TOKENS);
  }
  
  return 1;
}

void rm_empty_dir(){
  /*
  * This function is responsible for freeing data block of empty directory type file
  */
  unsigned int i,buffer[BLOCK_SIZE/4];
  for (i = 0; i < BLOCK_SIZE/4; i++) 
    buffer[i] = 0;
  add_block_to_free_list(fileInode.addr[0], buffer);

  // save superblock to system
  lseek(f_des, BLOCK_SIZE, SEEK_SET);
  write(f_des, &superBlock, BLOCK_SIZE);
}

void rmPlainFile(){
  /*
  * This function is responsible for freeing data block of plain type file
  */
  unsigned int i,buffer[BLOCK_SIZE/4];
  for (i = 0; i < BLOCK_SIZE/4; i++) 
    buffer[i] = 0;

  for (i = 0; i < ADDRESSES_SIZE; i++){
    if (fileInode.addr[i] != 0)
      add_block_to_free_list(fileInode.addr[i],buffer);
  }

  // save superblock to system
  lseek(f_des, BLOCK_SIZE, SEEK_SET);
  write(f_des, &superBlock, BLOCK_SIZE);
}

unsigned int change_dir(char *parameters){
  /*
  * change the global variable "currDirPath"
  * support cd .././.. or /user/dir1/dir2
  */
  if (parameters == NULL)
    return 1;
  
  char *dir_name, existFilename[MAX_FILE_TOKENS], copySys[15];
  dir_name = strtok_r(NULL, "/", &parameters);
  if(strlen(dir_name) > MAX_FILE_TOKENS){
    printf("Directory names cannot exceed 14 characters");
    return 0;
  }
  if (parameters != NULL && parameters[0]=='\0')
    parameters = NULL;
  
  /*printf("commands: %s\n",parameters);
  printf("Executing directory name: %s\n",dir_name);
  printf("Left commands: %s\n\n",parameters);*/

  unsigned short tmp_dir_inode_entry = dir_inode_entry;
  
  strcpy(copySys,v6FileName);
  if (strcmp(strcat(copySys,":"), dir_name) == 0){

    // if the directory is the system name, it starts from root 
    edit_fl_name(v6FileName);
    
    if (!change_dir(parameters))
      return 0;

  } else {

    // go to current directory
    unsigned short countDir = 0;          // used to jump to the next dirInode.addr[] data block
    unsigned short num_dir_inode_addr = 0;

    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE+2, 0);
    read(f_des, existFilename, MAX_FILE_TOKENS);

    while(existFilename[0] != '\0'){
      if (strcmp(existFilename, dir_name) == 0){
        unsigned short flag;
        lseek(f_des, 2*BLOCK_SIZE+(dir_inode_entry-1)*INODE_SIZE,0);
        read(f_des,&flag,2);
        if(flag >= (inode_alloc | dir_flag)){
          printf("\nFilename of the v6-directory is found!\n");
          readDirInode(dir_inode_entry);

          if (strcmp(dir_name, ".") == 0) // directory itself, do nothing
            break;
          else if (strcmp(dir_name, "..") == 0){
            // parent directory
            unsigned short L = strlen(currDirPath),i;
            currDirPath[L-1] = '\0';
            for (i = L-2; i >= 0; i--){
              if (currDirPath[i] == '/')
                break;
              else if (currDirPath[i] == ':'){
                currDirPath[i+1] = '/';
                break;
              }
              currDirPath[i] = '\0';
            }

            break;
          }

          // Add on to the currDirPath
          unsigned short L1 = strlen(currDirPath), L2 = strlen(dir_name),i;
          for (i = 0; i < L2; i++){
            currDirPath[i+L1] = dir_name[i];
          }

          currDirPath[L1+L2] = '/';

          break;
        }
        else {
          printf("\nFilename is v6-file, but not directory type!Keep finding......\n");
        }
      }
      
      lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + (++countDir)*16, SEEK_SET);

      if (countDir == BLOCK_SIZE / 16){
        // when number of data blocks > 1
        countDir = 0;
        lseek(f_des, dirInode.addr[++num_dir_inode_addr]*BLOCK_SIZE, SEEK_SET);
      }

      read(f_des, &dir_inode_entry, 2);
      read(f_des, existFilename, MAX_FILE_TOKENS);
    }

    if (existFilename[0] == '\0' || !change_dir(parameters)){
      readDirInode(tmp_dir_inode_entry);
      return 0;
    }
  
  }
  
  return 1;
}

void show_fl_dir(){
  /*
  * display all the files (including directory and plain files in current directory)
  */

  char existFilename[MAX_FILE_TOKENS];
  unsigned short countDir = 2;          // start from the 3rd file
  unsigned short num_dir_inode_addr = 0;
  unsigned short tmp_inode_entry;

  lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE+32, 0);
  read(f_des, &tmp_inode_entry, 2);
  read(f_des, existFilename, MAX_FILE_TOKENS);

  while(existFilename[0] != '\0'){
    if (countDir%4 == 2)
      printf("\n");

    if (countDir == BLOCK_SIZE / 16){
      // when number of data blocks > 1
      countDir = 0;
      lseek(f_des, dirInode.addr[++num_dir_inode_addr]*BLOCK_SIZE, SEEK_SET);
    }
    
    unsigned short flag;
    lseek(f_des, 2*BLOCK_SIZE+(tmp_inode_entry-1)*INODE_SIZE,0);
    read(f_des,&flag,2);
    if(flag >= (inode_alloc | dir_flag)){
      printf("%s(dir type)    ",existFilename);
    }
    else {
      printf("%s    ",existFilename);
    }
    
    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE+(++countDir)*16, 0);

    read(f_des, &tmp_inode_entry, 2);
    read(f_des, existFilename, MAX_FILE_TOKENS);
  }

}

int init_help(char *parameters){
  /* 
  * Decode the input parameters
  * finish initializing the v6 file system 
  */

  char *n1, *n2;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  
  parameters = strtok(NULL, " ");
  filepath = parameters;
  parameters = strtok(NULL, " ");
  n1 = parameters;
  parameters = strtok(NULL, " ");
  n2 = parameters;
  
  if(access(filepath, F_OK) != -1) {
      
    if(f_des = open(filepath, O_RDWR, 0600) == -1){
    
        printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
        return 1;
    }
    printf("filesystem already exists and the same will be used.\n");
  
  } else {
  
    if (!n1 || !n2)
        printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
      
    else {
        numBlocks = atoi(n1);
        numInodes = atoi(n2);
        
        if( initfs(filepath, numBlocks, numInodes )){
          char *systemName = strrchr(filepath, '/');
          edit_fl_name(filepath);
          printf("The file system is initialized\n");	
        } else {
          printf("Error initializing file system. Exiting... \n");
          return 1;
        }
    }
  }
}

int initfs(char* path, unsigned short blocks,unsigned short inodes) {
  /* 
  * finish initializing the v6 file system 
  * create superBlock and corresponding INode and data block
  * create buffer and reuse them to write data block
  */
  
  unsigned int buffer[BLOCK_SIZE/4];
  int bytes_written;
  
  // superblock creation
  unsigned short i = 0;
  superBlock.fsize = blocks;
  unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE; // = 16
  
  if((inodes%inodes_per_block) == 0)
    superBlock.isize = inodes/inodes_per_block;
  else
    superBlock.isize = (inodes/inodes_per_block) + 1;
  
  if((f_des = open(path,O_RDWR|O_CREAT,0700))== -1) {
    printf("\n open() failed with the following error [%s]\n",strerror(errno));
    return 0;
  }
      
  for (i = 0; i < FREE_BLOCK_SIZE; i++)
    superBlock.free[i] =  0;			//initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.
  
  superBlock.nfree = 0;
  superBlock.ninode = 200;
  if (inodes <= 200)
    superBlock.ninode = inodes;
  
  for (i = 0; i < superBlock.ninode; i++)
    superBlock.inode[i] = superBlock.ninode - i;		//Initializing the inode array to corresponding inode numbers
  
  superBlock.flock = 'a'; 					//flock,ilock and fmode are not used.
  superBlock.ilock = 'b';					
  superBlock.fmod = 0;
  superBlock.time[0] = 0;
  superBlock.time[1] = 1970;
  
  // writing zeroes to reusable buffer
  for (i = 0; i < BLOCK_SIZE/4; i++) 
    buffer[i] = 0;
      
  for (i = 0; i < superBlock.isize; i++)
    write(f_des, buffer, BLOCK_SIZE);
  
  int data_blocks = blocks - 2 - superBlock.isize;
  int data_blocks_for_free_list = data_blocks - 1;
  
  // Create root directory
  create_root();

  for ( i = 0; i < data_blocks_for_free_list; i++ ) {
    add_block_to_free_list(i + 2 + superBlock.isize + 1, buffer);
  }
  
  lseek(f_des, BLOCK_SIZE, SEEK_SET);
  write(f_des, &superBlock, BLOCK_SIZE); // writing superblock to file system
  
  return 1;
}

void add_block_to_free_list(int block_number,  unsigned int *empty_buffer){
  /* 
  * Add Data blocks to free list 
  * change nfree and free in superBlock simultaneously
  */

  if ( superBlock.nfree == FREE_BLOCK_SIZE ) {

    int free_list_data[BLOCK_SIZE / 4], i;
    free_list_data[0] = FREE_BLOCK_SIZE;
    
    for ( i = 0; i < BLOCK_SIZE / 4; i++ ) {
       if ( i < FREE_BLOCK_SIZE ) {
         free_list_data[i + 1] = superBlock.free[i];
       } else {
         free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
       }
    }
    
    lseek( f_des, (block_number) * BLOCK_SIZE, 0 );
    write( f_des, free_list_data, BLOCK_SIZE ); // Writing free list to header block
    
    superBlock.nfree = 0;
    
  } else {

	  lseek( f_des, (block_number) * BLOCK_SIZE, 0 );
    write( f_des, empty_buffer, BLOCK_SIZE );  // writing 0 to remaining data blocks to get rid of junk data
  }

  superBlock.free[superBlock.nfree] = block_number;  // Assigning blocks to free array
  ++superBlock.nfree;
}


void create_root() {
  /* 
  * Create root directory 
  * create superBlock and corresponding INode and directory entry in root data block
  */

  int root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
  int i;
  
  root.inode = getFreeInode();   // root directory's inode number is 1.
  root.filename[0] = '.';
  root.filename[1] = '\0';
  
  dirInode.flags = inode_alloc | dir_flag | dir_large_file | dir_access_rights;   		// flag for root directory 
  dirInode.nlinks = 0; 
  dirInode.uid = 0;
  dirInode.gid = 0;
  dirInode.size = INODE_SIZE; // 64 bits (if 32 bits => 4GB)
  dirInode.addr[0] = root_data_block;
  
  for( i = 1; i < ADDRESSES_SIZE; i++ ) {
    dirInode.addr[i] = 0;
  }
  
  dirInode.actime[0] = 0;
  dirInode.modtime[0] = 0;
  dirInode.modtime[1] = 0;
  
  lseek(f_des, 2 * BLOCK_SIZE, 0);
  write(f_des, &dirInode, INODE_SIZE);
  
  lseek(f_des, root_data_block*BLOCK_SIZE, 0);
  write(f_des, &root, 16);
  
  root.filename[0] = '.';
  root.filename[1] = '.';
  root.filename[2] = '\0';
  
  write(f_des, &root, 16);
 
}

void showSuper(){
  /*
   * display the superBlock info
  */

  printf("SuperBlock info \n\n");

  printf("superBlock.isize: %hu\n", superBlock.isize);
  printf("superBlock.fsize: %hu\n", superBlock.fsize);
  printf("superBlock.nfree: %hu\n", superBlock.nfree);
  printf("superBlock.free[%hu]: %i\n", superBlock.nfree-1, superBlock.free[superBlock.nfree-1]);
  printf("superBlock.ninode: %hu\n", superBlock.ninode);
  printf("superBlock.inode: %hu\n", superBlock.inode[superBlock.ninode-1]);
  printf("superBlock.flock: %c\n", superBlock.flock);
  printf("superBlock.ilock: %c\n", superBlock.ilock);
  printf("superBlock.fmod: %hu\n", superBlock.fmod);
  printf("superBlock.time[1]: %hu\n\n", superBlock.time[1]);
}


void readSuper(){
  /*
   * load the superBlock info
  */

  lseek(f_des, BLOCK_SIZE, 0);
  
  read(f_des, &superBlock.isize, 2);
  read(f_des, &superBlock.fsize, 2);
  read(f_des, &superBlock.ninode, 2);
  read(f_des, &superBlock.nfree, 2);
  read(f_des, &superBlock.free, 4*FREE_BLOCK_SIZE);
  read(f_des, &superBlock.inode, 2*200);
  read(f_des, &superBlock.flock, 1);
  read(f_des, &superBlock.ilock, 1);
  read(f_des, &superBlock.fmod, 2);
  read(f_des, &superBlock.time, 2*2);
}

void readDirInode(unsigned short entry){
  /*
   * load the directory I-node info according to the I-node number
  */

  lseek(f_des, 2*BLOCK_SIZE+INODE_SIZE*(entry-1), SEEK_SET);

  read(f_des, &dirInode.flags, 2);
  read(f_des, &dirInode.nlinks, 2);
  read(f_des, &dirInode.uid, 2);
  read(f_des, &dirInode.gid, 2);
  read(f_des, &dirInode.size, 4);
  read(f_des, &dirInode.addr, 4*ADDRESSES_SIZE);
  read(f_des, &dirInode.actime, 4);
  read(f_des, &dirInode.modtime, 4);

  dir_inode_entry = entry;
}

void readFileInode(unsigned short entry){
  /*
   * load the file I-node info according to the I-node number
  */
  lseek(f_des, 2*BLOCK_SIZE+INODE_SIZE*(entry-1), SEEK_SET);

  read(f_des, &fileInode.flags, 2);
  read(f_des, &fileInode.nlinks, 2);
  read(f_des, &fileInode.uid, 2);
  read(f_des, &fileInode.gid, 2);
  read(f_des, &fileInode.size, 4);
  read(f_des, &fileInode.addr, 4*ADDRESSES_SIZE);
  read(f_des, &fileInode.actime, 4);
  read(f_des, &fileInode.modtime, 4);

}

void add_to_inode_list(unsigned short inode_entry){
  /*
  * This function is responsible for deleting inode and adding free inode entry to superblock
  */
  unsigned int i,buffer[INODE_SIZE/4];
  for (i = 0; i < INODE_SIZE/4; i++) 
    buffer[i] = 0;
  lseek(f_des, 2 * BLOCK_SIZE + (inode_entry-1)*INODE_SIZE, SEEK_SET);
  write(f_des, buffer, INODE_SIZE);
  superBlock.inode[superBlock.ninode++] = inode_entry;

  // save superblock to system
  lseek(f_des, BLOCK_SIZE, SEEK_SET);
  write(f_des, &superBlock, BLOCK_SIZE);
}

unsigned short getFreeInode(){
  /*
  * use superBlock.ninode and superBlock inode[]
  * assume the # combine plain file and directory type file is no more than 
  * (= 200) 
  */
  
  if (superBlock.ninode == 0){
    printf("The inode blocks are full!\n");
    return -1;
  }
  unsigned short fetchInode;
  fetchInode = superBlock.inode[--superBlock.ninode];
  
  return fetchInode;
}

unsigned int getFreeBlock(){
  /*
   * get the new free data block
   * if it's empty, go to next data block that stores indexes of free data block 
  */

  --superBlock.nfree;
  int acquired_free_data_block = superBlock.free[superBlock.nfree];
  if (superBlock.nfree == 0){
    printf("check if nfree is empty!\n");
    if (superBlock.free[superBlock.nfree] == 0){
      printf("\n The v6 file system is full! Write failed! Abort...\n");
      return -1;
    }
    
    lseek(f_des, acquired_free_data_block*BLOCK_SIZE, SEEK_SET);
    read(f_des, &superBlock.nfree, 2);
    read(f_des, &superBlock.free, 4*FREE_BLOCK_SIZE);

    return superBlock.free[--superBlock.nfree];
    
    // free the block originally contain indexes of free blocks
    // add_block_to_free_list(index_data_block, buffer) 
    
  }

  return acquired_free_data_block;
}

int copyIn(char *parameters) {
  /* 
  * copy external file into v6 file 
  * check if the v6-filename already exist
  * open external file and check if exist
  * create a new v6 file in file system 
  * copy the content byte by byte from external file into v6 file
  * update superBlock and store new v6 file I-node
  * Have to support user/dir1/v6file
  */

  char *extFilePath, *vFilePath;

  parameters = strtok(NULL, " ");
  extFilePath = parameters;
  parameters = strtok(NULL, " ");
  vFilePath = parameters;

  // Decompose the vFilePath
  short i,j = 0, pos_last_slash = -1, l = strlen(vFilePath);

  for (i = l-1; i >= 0; i--){
    if(vFilePath[i] == '/'){
      pos_last_slash = i;
      break;
    }
  }
  
  if(l-pos_last_slash-1 > MAX_FILE_TOKENS){
    printf("v6 filename cannot exceed 14 characters");
    return 0;
  }

  char *vFile = malloc(l-pos_last_slash);
  for (i = pos_last_slash + 1; i < l; i++){
    vFile[j] = vFilePath[i];
    j++;
  }

  for (j; j < strlen(vFile); j++)
    vFile[j] = '\0';

  if (pos_last_slash != -1){
    char *path = malloc(pos_last_slash+1);
    for (i = 0; i < pos_last_slash; i++){
      path[i] = vFilePath[i];
    }

    for (i; i < strlen(path); i++)
      path[i] = '\0';
    
    if(!change_dir(path))
      return 0;
  }

  /*printf("%hu, %hu\n", pos_last_slash, l);
  printf("%s\n", path);
  printf("%s\n", vFile);
  printf("%s\n", vFilePath);*/
  
  if((f_des = open(v6FileName,O_RDWR,0700))== -1){
    printf("\n v6 file system open() failed with the following error [%s]\n",strerror(errno));
    return 0;
  }
  
  // load the superblock data
  readSuper();
  
  // go to first data block to see if the v6-filename already exist (future improvement: handle number of root data blocks > 1)
  char existFilename[MAX_FILE_TOKENS];
  unsigned short file_entry;
  unsigned int countDir = 0;            // used ti jump to the next dirInode.addr[] data block adn track the position in current data block
  unsigned short num_dir_inode_addr = 0;

  lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE, 0);
  read(f_des, &file_entry, 2);
  read(f_des, existFilename, MAX_FILE_TOKENS);
  
  while(existFilename[0] != '\0'){
    //printf("Existing filename: %s\n",existFilename);
    if (strcmp(existFilename, vFile) == 0){
      unsigned short flag;
      lseek(f_des, 2*BLOCK_SIZE+(file_entry-1)*INODE_SIZE,0);
      read(f_des,&flag,2);
      if(flag >= (inode_alloc | dir_flag)){
        printf("\nFilename is directory type. Keep finding...\n");
      }
      else {
        printf("\nFilename of the v6-File is already exist! copy in failed! Abort....\n");
        return 0;
      }
    }

    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + (++countDir)*16, 0);

    if (countDir == BLOCK_SIZE / 16){
      // only when number of data blocks > 1
      countDir = 0;
      lseek(f_des, dirInode.addr[++num_dir_inode_addr]*BLOCK_SIZE, SEEK_SET);
    }

    lseek(f_des, 2, SEEK_CUR);
    read(f_des, existFilename, MAX_FILE_TOKENS);
  }

  // open the external file and check if exist
  int extFileDescrt;
  if((extFileDescrt = open(extFilePath,O_RDONLY))== -1){
    printf("\n external file open() failed with the following error [%s]\n",strerror(errno));
    return 0;
  }

  // initialize the v6-file data block and inode
  unsigned int vFile_data_block = getFreeBlock(); // should look up superBlock.free
  if (vFile_data_block == -1){
    printf("\n The v6 file system is full!\n");
    return 0;
  }
  
  inode_type file_inode;
  file_inode.flags = inode_alloc | plain_file_flag | dir_access_rights; // flag for new plain small file
  file_inode.nlinks = 0;
  file_inode.uid = 0;
  file_inode.gid = 0;
  file_inode.size = INODE_SIZE;
  file_inode.addr[0] = vFile_data_block;
  
  for( i = 1; i < ADDRESSES_SIZE; i++ ) {
    file_inode.addr[i] = 0;
  }
  
  file_inode.actime[0] = 0;
  file_inode.modtime[0] = 0;
  file_inode.modtime[1] = 0;
  
  // copy the content, update nfree and free in superblock (update 'addr' in v6File inode)
  char ch[1];
  int curr_block_bytes = 0;
  short num_addr = 1;
  lseek(f_des, vFile_data_block*BLOCK_SIZE, SEEK_SET);
  while (read(extFileDescrt, ch, 1)) { 
    if (curr_block_bytes == BLOCK_SIZE){
      // if the current data block is full
      vFile_data_block = getFreeBlock();
      if (vFile_data_block == -1){
        printf("\n The v6 file system is full!\n");
        return 0;
      }
      file_inode.addr[num_addr] = vFile_data_block;
      ++num_addr;
      curr_block_bytes = 0;
      lseek(f_des, vFile_data_block*BLOCK_SIZE, SEEK_SET);
    }

    write(f_des, ch, 1);
    ++curr_block_bytes;
  }
  
  // update the directory in directory data block
  dir_type vFile_dir;
  unsigned short inode_entry = getFreeInode();
  if (inode_entry == -1){
    printf("\n The v6 file system is running out of I-Node entry!\n");
    return 0;
  }

  vFile_dir.inode = inode_entry;
  for (i = 0; i < MAX_FILE_TOKENS; i++){
    if (vFile[i] == '\0'){
      vFile_dir.filename[i] = '\0';
      break;
    }
    vFile_dir.filename[i] = vFile[i];
  }

  // modify the i-node of the directory (specifically only its addr, because the number of file in the root could be larger than 64)
  if (countDir == 0){
    // the current root data block happens to be full, get the new free block for directory
    // num_dir_inode_addr: current number of full data blocks that belongs to the directory
    // update Inode of the directory
    
    unsigned int new_data_block = getFreeBlock();
    lseek(f_des, 2 * BLOCK_SIZE + (dir_inode_entry-1)*INODE_SIZE + 12 + 4*num_dir_inode_addr, 0);
    write(f_des, &new_data_block, 4);

    lseek(f_des, new_data_block*BLOCK_SIZE, 0);
    write(f_des, &vFile_dir, 16);
  } else {
    // the current root data block is not full
    // num_dir_inode_addr: current number of full data blocks that belongs to the directory
    // locate dirInode.addr[num_dir_inode_addr]
    // update data block of the directory

    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE+countDir*16, 0);
    write(f_des, &vFile_dir, 16);
  }
  
  // store new v6-file inode
  lseek(f_des, 2 * BLOCK_SIZE + (inode_entry-1)*INODE_SIZE, 0);
  write(f_des, &file_inode, INODE_SIZE);
  
  // update superBlock
  lseek(f_des, BLOCK_SIZE, SEEK_SET);
  write(f_des, &superBlock, BLOCK_SIZE);

  return 1;
}

int copyOut(char *parameters) {
  /* 
  * copy v6 file out to an external file 
  * check if the v6-filename exist
  * open external file and overwrite it
  * copy the content byte by byte from v6 file out to the external file
  * stop when enters '\0' character
  */

  char *extFilePath, *vFilePath;

  parameters = strtok(NULL, " ");
  vFilePath = parameters;
  parameters = strtok(NULL, " ");
  extFilePath = parameters;
  
  // Decompose the vFilePath
  short i,j = 0, pos_last_slash = -1, l = strlen(vFilePath);

  for (i = l-1; i >= 0; i--){
    if(vFilePath[i] == '/'){
      pos_last_slash = i;
      break;
    }
  }
  
  if(l-pos_last_slash-1 > MAX_FILE_TOKENS){
    printf("v6 filename cannot exceed 14 characters");
    return 0;
  }

  char *vFile = malloc(l-pos_last_slash);
  for (i = pos_last_slash + 1; i < l; i++){
    vFile[j] = vFilePath[i];
    j++;
  }
  
  for (j; j < strlen(vFile); j++)
    vFile[j] = '\0';
  
  if (pos_last_slash != -1){
    char *path = malloc(pos_last_slash+1);
    for (i = 0; i < pos_last_slash; i++){
      path[i] = vFilePath[i];
    }

    for (i; i < strlen(path); i++)
      path[i] = '\0';
    
    /*printf("%s\n", path);
    printf("%h, %h\n", pos_last_slash, l);
    printf("%s\n", vFile);
    printf("%s\n", vFilePath);*/

    if(!change_dir(path))
      return 0;
  }
  
  
  if((f_des = open(v6FileName,O_RDWR,0700))== -1){
    printf("\n v6 file system open() failed with the following error [%s]\n",strerror(errno));
    return 0;
  }
  
  // load the superblock data
  readSuper();

  // go to first data block to see if the v6-filename exist
  char existFilename[MAX_FILE_TOKENS];
  
  unsigned short file_inode_entry;
  unsigned short countDir = 0;          // used to jump to the next dirInode.addr[] data block
  unsigned short num_dir_inode_addr = 0;
  
  readDirInode(dir_inode_entry);

  lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + 2, 0);
  read(f_des, existFilename, MAX_FILE_TOKENS);

  while(existFilename[0] != '\0'){
    //printf("Existing filename: %s\n",existFilename);
    if (strcmp(existFilename, vFile) == 0){
      unsigned short flag;
      lseek(f_des, 2*BLOCK_SIZE+(file_inode_entry-1)*INODE_SIZE,0);
      read(f_des,&flag,2);
      if(flag >= (inode_alloc | dir_flag)){
        printf("\nFilename is directory type. Keep finding...\n");
      }
      else {
        printf("\nFilename of the v6-File is found! Ready to copy out!\n");
        break;
      }
    }

    lseek(f_des, dirInode.addr[num_dir_inode_addr]*BLOCK_SIZE + (++countDir)*16, 0);

    if (countDir == BLOCK_SIZE / 16){
      // only when number of data blocks > 1
      countDir = 0;
      lseek(f_des, dirInode.addr[++num_dir_inode_addr]*BLOCK_SIZE, SEEK_SET);
    }

    read(f_des, &file_inode_entry, 2);
    read(f_des, existFilename, MAX_FILE_TOKENS);
  }

  // if no such file name exist in v6 file system, break
  if (existFilename[0] == '\0'){
    printf("\nFilename of the v6-File is not found! Abort...\n");
    return 0;
  }
  
  // create the new external file
  int extFileDescrt;
  extFileDescrt = open(extFilePath,O_RDWR|O_CREAT,0700);
  
  // read the inode entry of the v6-file and get the addr array
  readFileInode(file_inode_entry);

  // copy the contentinto the new external file
  char ch[1];
  int curr_block_bytes = 0;
  unsigned short num_addr = 0;
  lseek(f_des, fileInode.addr[0]*BLOCK_SIZE, SEEK_SET);
  read(f_des, ch, 1);
  while (ch[0] != 0) { 
    write(extFileDescrt, ch, 1);
    ++curr_block_bytes;
    
    if (curr_block_bytes == BLOCK_SIZE){
      // if the current data block is copied completely
      ++num_addr;
      if (fileInode.addr[num_addr] == 0)
        break;
      lseek(f_des, fileInode.addr[num_addr]*BLOCK_SIZE, SEEK_SET);
      curr_block_bytes = 0;
    }
    
    read(f_des, ch, 1);
  }

  return 1;
}