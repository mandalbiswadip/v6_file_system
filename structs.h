#include "constants.h"
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
unsigned int addr[INODE_ADDRESSES];
unsigned int actime;
unsigned int modtime;
} inode_type;
inode_type first_inode;

// directory
typedef struct { 
    unsigned int inode; 
    char filename[MAX_FILE_TOKENS];
} dir_type;

