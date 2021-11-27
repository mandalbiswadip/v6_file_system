#include "constants.h"
#include "structs.h"
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

unsigned short block_number_tracker[max_array];



void bytes_to_inode(char *data, inode_type *inode) {
    memcpy(&inode->flags, &data[0], 2);
    memcpy(&inode->nlinks, &data[2], 2);
    memcpy(&inode->uid, &data[4], 4);
    memcpy(&inode->gid, &data[8], 4);
    memcpy(&inode->size0, &data[12], 4);
    memcpy(&inode->size1, &data[16], 4);
    memcpy(&inode->addr, &data[20], 36);
    memcpy(&inode->actime, &data[56], 4);
    memcpy(&inode->modtime, &data[60], 4);
}


void inode_to_bytes(inode_type *inode, char *data) {
    memcpy(&data[0], &inode->flags, 2);
    memcpy(&data[2], &inode->nlinks, 2);
    memcpy(&data[4], &inode->uid, 4);
    memcpy(&data[8], &inode->gid, 4);
    memcpy(&data[12], &inode->size0, 4);
    memcpy(&data[16], &inode->size1, 4);
    memcpy(&data[20], &inode->addr, 36);
    memcpy(&data[56], &inode->actime, 4);
    memcpy(&data[60], &inode->modtime, 4);
}

void bytes_to_directory(char *data, dir_type *direc) {
    memcpy(&direc->inode, &data[0],  4);
    memcpy(&direc->filename, &data[4], 28);
}

void bytes_to_superblock(char *data, superblock_type *sp) {
    memcpy(&sp->isize, &data[0],  4);
    memcpy(&sp->fsize, &data[4], 4);
    memcpy(&sp->nfree, &data[8], 4);
    memcpy(&sp->free, &data[12], 1004);
    memcpy(&sp->flock, &data[1016], 1);
    memcpy(&sp->ilock, &data[1017], 1);
    memcpy(&sp->fmod, &data[1018], 1);
    memcpy(&sp->time, &data[1019], 4);
}
