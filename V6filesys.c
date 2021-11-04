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
#define BLOCK_SIZE 512		 // * 1024
#define ISIZE 32		 // * 64
#define inode_alloc 0100000        //Flag value to allocate an inode
#define pfile 000000               //To define file as a plain file
#define lfile 010000               //To define file as a large file
#define directory 040000           //To define file as a Directory
#define max_array 256
#define null_size 512		// * check where it's getting used


//GLOBAL VARIABLES
int fd ;                
int rootfd;
const char *rootname;
unsigned short chainarray[max_array];



// super block 
typedef struct {
unsigned short isize;
unsigned short fsize;
unsigned short nfree;
unsigned short free[100]; // * 251 length
unsigned short ninode;	  // * need to remove
unsigned short inode[100];	// * need to remove
char flock;
char ilock;
char fmod;
unsigned short time[2]; 	// * only unsigned int
} fs_super;//instance of superblock
fs_super super;


//inode

typedef struct {
unsigned short flags;
char nlinks;		// * unsigned short
char uid;   // --> * unsigned int
char gid;  // --> * unsigned int
char size0;  // * --> unsigned int
unsigned short size1;    // * --> unsigned int
unsigned short addr[8];		// * size is 9   --> unsigned int
unsigned short actime[2];	// * not an array --> unsigned int
unsigned short modtime[2];	// * not an array --> unsigned int
} fs_inode; 
fs_inode initial_inode;/

// directory
typedef struct 
{
        unsigned short inode;	// * unsigned int
        char filename[13];     // * size is 28
        }dir;
dir newdir;		
dir dir1;   //for duplicates

//nums of inodes, NUM is the number of files of this file system
//struct inode g_usedinode[NUM];


//in order

void chainblocks( unsigned short total_blocks);
void rootdir();
int initializefs(char* path, unsigned short total_blocks,unsigned short total_inodes);
unsigned short allocatefreedblock();

void read_block(unsigned short *dest, unsigned short bno);
void write_block(unsigned short *dest, unsigned short bno);
void copy_inode(fs_inode current_inode, unsigned int new_inode);






// Data blocks chaining procedure 
 void chainblocks(unsigned short total_blocks)
	 {
	 unsigned short emptybuffer[256];
	 unsigned short linked_blockcount;
	 unsigned short split_blocks = total_blocks/100;
	 unsigned short remaining_blocks = total_blocks%100;
	 unsigned short idx = 0;
	 int i=0;
	 for (idx=0; idx <= 255; idx++)
		 {
		 emptybuffer[idx] = 0;
		 chainarray[idx] = 0;
		 }

	 //chaining for chunks of blocks 100 blocks at a time
	 for (linked_blockcount=0; linked_blockcount < split_blocks; linked_blockcount++)
		 {
		 chainarray[0] = 100;
	
		 for (i=0;i<100;i++)
			 {
			 if((linked_blockcount == (split_blocks - 1)) && (remaining_blocks == 0))
				 {
				 if ((remaining_blocks == 0) && (i==0))
					 {
					 if ((linked_blockcount == (split_blocks - 1)) && (i==0))
						 {
						 chainarray[i+1] = 0;
						 continue;
						 }
					 }
				 }
			 chainarray[i+1] = i+(100*(linked_blockcount+1))+(super.isize + 2 );
			 }			

		 write_block(chainarray, 2+super.isize+100*linked_blockcount);

		 for (i=1; i<=100;i++)
			 write_block(emptybuffer, 2+super.isize+i+ 100*linked_blockcount);
		 }

	//chaining for remaining blocks
	chainarray[0] = remaining_blocks;
	chainarray[1] = 0;
	
	for (i=1;i<=remaining_blocks;i++)
		 chainarray[i+1] = 2+super.isize+i+(100*linked_blockcount);

	 write_block(chainarray, 2+super.isize+(100*linked_blockcount));

	 for (i=1; i<=remaining_blocks;i++)
		 write_block(chainarray, 2+super.isize+1+i+(100*linked_blockcount));
	 }


 //create root directory and inode. 
 void rootdir()
 	{
 	rootname = "root";
 	rootfd = creat(rootname, 0600);
 	rootfd = open(rootname , O_RDWR | O_APPEND);
 	unsigned int i = 0;
 	unsigned short num_bytes;
 	unsigned short dblock = allocatefreedblock();
 	
	for (i=0;i<14;i++)
		 newdir.filename[i] = 0;

 	newdir.filename[0] = '.';                       //root directory's file name is .
 	newdir.filename[1] = '\0';
 	newdir.inode = 1;                                       // root directory's inode number is 1.
	
	 initial_inode.flags = inode_alloc | directory | 000077;
	 initial_inode.nlinks = 2;
	 initial_inode.uid = '0';
	 initial_inode.gid = '0';
	 initial_inode.size0 = '0';
	 initial_inode.size1 = ISIZE;
	 initial_inode.addr[0] = dblock;
	
	 for (i=1;i<8;i++)
		 initial_inode.addr[i] = 0;

	initial_inode.actime[0] = 0;
	initial_inode.modtime[0] = 0;
	initial_inode.modtime[1] = 0;

	copy_inode(initial_inode, 0);
	lseek(rootfd , 512 , SEEK_SET);
	write(rootfd , &initial_inode , 32);
	lseek(rootfd, dblock*BLOCK_SIZE, SEEK_SET);

	//filling 1st entry with .
	num_bytes = write(rootfd, &newdir, 16);
	if((num_bytes) < 16)
		 printf("\n Error in writing root directory \n ");

	//filling net entry with ..
	newdir.filename[0] = '.';
	newdir.filename[1] = '.';
	newdir.filename[2] = '\0';
	num_bytes = write(rootfd, &newdir, 16);
	if((num_bytes) < 16)
		 printf("\n Error in writing root directory\n ");
	close(rootfd);
	}




// file system initilization
int initializefs(char* path, unsigned short total_blocks,unsigned short total_inodes)
	{
	printf("\nV6 File System by Devanshu and Sri Chandra Kiran \n");
	char buffer[BLOCK_SIZE];
	int num_bytes;
	if(((total_inodes*32)%512) == 0) //the inode is 32 byte long so each block contains 16 inode
		super.isize = ((total_inodes*32)/512);//number of inode
	else
		super.isize = ((total_inodes*32)/512)+1;	

	super.fsize = total_blocks;

	unsigned short i = 0;
	
	if((fd = open(path,O_RDWR|O_CREAT,0600))== -1)          
		{
		printf("\n open() failed with error [%s]\n",strerror(errno));
		return 1;
		}
	//assigning superblock values
	for (i = 0; i<100; i++)
		super.free[i] =  0;			

	super.nfree = 0;//number of free block
	super.ninode = 100;//free inode number
	
	for (i=0; i < 100; i++)
		super.inode[i] = i;		

	super.flock = 'f'; 					
	super.ilock = 'i';					
	super.fmod = 'f';
	super.time[0] = 0000;
	super.time[1] = 1970;
	lseek(fd,BLOCK_SIZE,SEEK_SET);
	//#lseek(fd,0,SEEK_SET);
	//#write(fd, &super, 512);// write 513 byte from &super to fd

	if((num_bytes =write(fd,&super,BLOCK_SIZE)) < BLOCK_SIZE)
		{
		printf("\n error in writing the super block\n");
		return 0;
		}

	for (i=0; i<BLOCK_SIZE; i++)  
		buffer[i] = 0;

	lseek(fd,i*512,SEEK_SET);//reposition read/write file offset i*512 byte from the start of file

	for (i=0; i < super.isize; i++)
		write(fd,buffer,BLOCK_SIZE);//write zero to all inode

	// chaining data blocks
	chainblocks(total_blocks);	

	for (i=0; i<100; i++) 
		{
		super.free[super.nfree] = i+2+super.isize; //get free block
		++super.nfree;
		}

	rootdir();
	return 1;
	}

// get a free data block.&&
unsigned short allocatefreedblock()
	{
	unsigned short block;
	block = super.free[--super.nfree];
	super.free[super.nfree] = 0;
	
	if (super.nfree == 0)
		{
		int n=0;
		read_block(chainarray, block);
		super.nfree = chainarray[0];
		for(n=0; n<100; n++)
			super.free[n] = chainarray[n+1];
		}
	return block;
	}





//Read integer array from the required block&&
void read_block(unsigned short *dest, unsigned short bno)
	{
	int flag=0;
	if (bno > super.isize + super.fsize )
		flag = 1;

	else
		{			
		lseek(fd,bno*
		      
		      
		      ,SEEK_SET);
		read(fd, dest, BLOCK_SIZE);
		}
	}


//Write integer array to the required block
void write_block(unsigned short *dest, unsigned short bno)
	{
	int flag1, flag2;
	int num_bytes;
	
	if (bno > super.isize + super.fsize )
		flag1 = 1;		
	else
		{
		lseek(fd,bno*BLOCK_SIZE,0);
		num_bytes=write(fd, dest, BLOCK_SIZE);

		if((num_bytes) < BLOCK_SIZE)
			flag2=1;		
		}
	if (flag2 == 1)
		{
		printf("problem with block!\n");
		}
	}

//Write to an inode given the inode number	&&
void copy_inode(fs_inode current_inode, unsigned int new_inode)
	{
	int num_bytes;
	lseek(fd,2*BLOCK_SIZE + new_inode*ISIZE,0);
	num_bytes=write(fd,&current_inode,ISIZE);

	if((num_bytes) < ISIZE)
		printf("\n Error in inode number : %d\n", new_inode);		
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
	int i=0;
	char *tmp = (char *)malloc(sizeof(char) * 200);
	char *cmd1 = (char *)malloc(sizeof(char) * 200);
	signal(SIGINT, SIG_IGN);
	int fs_initcheck = 0;    // bit to check if file system is initialized
	char *parser, cmd[256];
	unsigned short n = 0, j , k;
	char buffer1[BLOCK_SIZE];
	unsigned short num_bytes;
	char *name_dir;
	char *cpoutextern;
	char *cpoutsource;
	unsigned int bno =0, inode_no=0;
	char *cpinextern;
	char *cpinsource;
	char *file_path;
	char *filename;
	char *num1, *num2, *num3, *num4;

	
	printf("\n\nInitialize file system by using initfs <<name of your v6filesystem>> << total blocks>> << total inodes>>\n");
	while(1)
		{
		printf("\nEnter command\n");
		scanf(" %[^\n]s", cmd);
		parser = strtok(cmd," "); //break string in to a serious tokens using the delimiter " "
		if(strcmp(parser, "initfs")==0)
			{
			file_path = strtok(NULL, " "); //read the second breaked tokens 
			num1 = strtok(NULL, " "); //read the third breaked tokens           
			num2 = strtok(NULL, " "); //read the forth breaked tokens 
			if(access(file_path,X_OK) != -1)
				{
				if( fd = open(file_path, O_RDWR) == -1)
					{
					printf("File system exists, open failed");
					return 1;
					}
				access(file_path, R_OK |W_OK | X_OK);
				printf("Filesystem already exists.\n");
				printf("Same file system to be used\n");
				fs_initcheck=1;
				}
			else //the else situation is access file failed 
				{
				if (!num1 || !num2) //if num1 or num2 equals to zero 
					printf("Enter all arguments in form initfs v6filesystem 5000 400.\n");
				else
					{
					bno = atoi(num1);//convert a string to an interger(total_blocks)
					inode_no = atoi(num2);//convert a string to an interger(total_inodes)
					if(initializefs(file_path,bno, inode_no))
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
		}
}



			
