#ifndef DIR_H
#define DIR_H

#include "inode.h"

struct directory {
    struct inode *inode;
    unsigned int offset;
};

struct directory_entry {
    unsigned int inode_num;
    char name[16];
};

struct directory *directory_open(int inode_num);
int directory_get(struct directory *dir, struct directory_entry *ent);
int directory_make(char *path);
void directory_close(struct directory *dir);
void ls(void);

#endif
