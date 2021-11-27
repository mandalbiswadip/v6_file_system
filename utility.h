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

void bytes_to_inode(char *data, inode_type *inode);
void inode_to_bytes(inode_type *inode, char *data);
void bytes_to_directory(char *data, dir_type *direc);
void bytes_to_superblock(char *data, superblock_type *sp);
