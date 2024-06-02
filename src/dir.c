#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/dir.h"
#include "../include/block.h"
#include "../include/pack.h"
#include "../include/common.h"
#include "../include/inode.h"
#include "../include/path.h"

struct directory *directory_open(int inode_num) {
    struct inode *inode = iget(inode_num);
    if (inode == NULL) {
        return NULL;
    }

    struct directory *dir = malloc(sizeof(struct directory));
    if (dir == NULL) {
        iput(inode);
        return NULL;
    }

    dir->inode = inode;
    dir->offset = 0;

    return dir;
}

int directory_get(struct directory *dir, struct directory_entry *ent) {
    if (dir->offset >= dir->inode->size) {
        return -1;  // End of directory
    }

    unsigned int block_index = dir->offset / BLOCK_SIZE;
    unsigned int offset_in_block = dir->offset % BLOCK_SIZE;

    if (block_index >= INODE_PTR_COUNT) {
        return -1;  // Block index out of range
    }

    int data_block_num = dir->inode->block_ptr[block_index];
    if (data_block_num < 0) {
        return -1;  // No data block allocated
    }

    unsigned char block[BLOCK_SIZE];
    if (bread(data_block_num, block) == NULL) {
        return -1;  // Failed to read block
    }

    unsigned char *entry_ptr = block + offset_in_block;

    ent->inode_num = read_u16(entry_ptr);
    strcpy(ent->name, (char *)(entry_ptr + 2));


    dir->offset += DIRECTORY_ENTRY_SIZE;

    return 0;  // Success
}

void directory_close(struct directory *dir) {
    iput(dir->inode);
    free(dir);
}

void ls(void) {
    struct directory *dir;
    struct directory_entry ent;

    dir = directory_open(0);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open directory.\n");
        return;
    }

    while (directory_get(dir, &ent) != -1) {
        printf("%d %s\n", ent.inode_num, ent.name);
    }

    directory_close(dir);
}

int directory_make(char *path) {
    if (path[0] != '/') {
        return -1;
    }

    char parent_path[256];
    char name[256];
    get_dirname(path, parent_path);
    get_basename(path, name);

    struct inode *parent_inode = namei(parent_path);
    if (parent_inode == NULL) {
        return -1;
    }

    struct inode *new_inode = ialloc();
    if (new_inode == NULL) {
        iput(parent_inode);
        return -1;
    }
    unsigned int new_inode_num = new_inode->inode_num;

    int block_num = alloc();
    if (block_num < 0) {
        iput(new_inode);
        iput(parent_inode);
        return -1;
    }

    unsigned char block[BLOCK_SIZE];
    memset(block, 0, BLOCK_SIZE);

    write_u16(block, new_inode_num);
    strcpy((char *)(block + 2), ".");

    write_u16(block + DIRECTORY_ENTRY_SIZE, parent_inode->inode_num);
    strcpy((char *)(block + DIRECTORY_ENTRY_SIZE + 2), "..");

    bwrite(block_num, block);

    new_inode->flags = 2;
    new_inode->size = DIRECTORY_ENTRY_SIZE * 2;
    new_inode->block_ptr[0] = block_num;
    for (int i = 1; i < INODE_PTR_COUNT; i++) {
        new_inode->block_ptr[i] = 0;
    }

    iput(new_inode);

    unsigned char parent_block[BLOCK_SIZE];
    int parent_block_num = parent_inode->block_ptr[0];
    if (bread(parent_block_num, parent_block) == NULL) {
        return -1;
    }

    unsigned int offset_in_block = parent_inode->size;
    write_u16(parent_block + offset_in_block, new_inode_num);
    strcpy((char *)(parent_block + offset_in_block + 2), name);

    bwrite(parent_block_num, parent_block);

    parent_inode->size += DIRECTORY_ENTRY_SIZE;
    iput(parent_inode);

    return 0;
}