#define CTEST_ENABLE
#include "../include/common.h"
#include "../include/image.h"
#include "../include/block.h"
#include "../include/ctest.h"
#include "../include/free.h"
#include "../include/inode.h"
#include "../include/dir.h"
#include "../include/mkfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to reset in-core inodes
void reset_incore() {
    incore_free_all();
}

// Function to test inode allocation
void test_inode_allocation() {
    struct inode *allocated_inode = ialloc();
    CTEST_ASSERT(allocated_inode != NULL, "Allocate an inode");
}

// Function to test block allocation
int test_block_allocation() {
    int allocated_block = alloc();
    CTEST_ASSERT(allocated_block != -1, "Allocate a block");
    return allocated_block;
}

// Function to test block write and read
void test_block_write_read(int allocated_block) {
    unsigned char block_write[BLOCK_SIZE];
    for (size_t i = 0; i < sizeof(block_write); i++) {
        block_write[i] = (unsigned char)(i % 256);
    }
    bwrite(allocated_block, block_write);
    CTEST_ASSERT(bread(allocated_block, block_write) != NULL, "Write block and read back");

    // Read back the data from the allocated block and verify
    unsigned char block_read[BLOCK_SIZE];
    bread(allocated_block, block_read);
    int blocks_match = 1;
    for (size_t i = 0; i < sizeof(block_read); i++) {
        if (block_read[i] != block_write[i]) {
            blocks_match = 0;
            break;
        }
    }
    CTEST_ASSERT(blocks_match, "Verify written block matches read block");
}

// Function to test set free and find free
void test_set_free_find_free(int allocated_block) {
    unsigned char free_block[BLOCK_SIZE];
    bread(FREE_MAP_BLOCK, free_block); 
    set_free(free_block, allocated_block, 0);  
    bwrite(FREE_MAP_BLOCK, free_block);
    int found_free = find_free(free_block);
    CTEST_ASSERT(found_free == allocated_block, "Free and find block");
}

// Function to test in-core functions
void test_incore_functions() {
    reset_incore();

    struct inode *free_inode = incore_find_free();
    CTEST_ASSERT(free_inode != NULL, "Should find a free inode");
    free_inode->ref_count = 1;  // Simulate it being used

    struct inode *next_free_inode = incore_find_free();
    CTEST_ASSERT(next_free_inode != NULL, "Should find the next free inode");
    CTEST_ASSERT(next_free_inode != free_inode, "Should not return the same inode again");

    struct inode *incore = get_incore_array();
    incore[0].inode_num = 1;
    incore[0].ref_count = 1;
    incore[1].inode_num = 2;
    incore[1].ref_count = 1;

    struct inode *found_inode = incore_find(1);
    CTEST_ASSERT(found_inode != NULL, "Should find inode with number 1");
    CTEST_ASSERT(found_inode->inode_num == 1, "Found inode should have inode number 1");

    struct inode *not_found_inode = incore_find(3);
    CTEST_ASSERT(not_found_inode == NULL, "Should not find inode with number 3");

    incore_free_all();
    for (int i = 0; i < MAX_SYS_OPEN_FILES; i++) {
        CTEST_ASSERT(incore[i].ref_count == 0, "All inodes should be free (ref_count == 0)");
    }
}

// Function to test inode read and write
void test_inode_read_write() {
    struct inode in;
    in.size = 1024;
    in.owner_id = 1000;
    in.permissions = 0644 & 0xFF;
    in.flags = 0;
    in.link_count = 1;
    for (int i = 0; i < INODE_PTR_COUNT; i++) {
        in.block_ptr[i] = i;
    }
    in.ref_count = 1;  // in-core only
    in.inode_num = 1;  // in-core only

    write_inode(&in);

    struct inode read_in;
    read_inode(&read_in, 1);

    CTEST_ASSERT(read_in.size == in.size, "Inode size should match");
    CTEST_ASSERT(read_in.owner_id == in.owner_id, "Owner ID should match");
    CTEST_ASSERT(read_in.permissions == in.permissions, "Permissions should match");
    CTEST_ASSERT(read_in.flags == in.flags, "Flags should match");
    CTEST_ASSERT(read_in.link_count == in.link_count, "Link count should match");
    for (int i = 0; i < INODE_PTR_COUNT; i++) {
        CTEST_ASSERT(read_in.block_ptr[i] == in.block_ptr[i], "Block pointers should match");
    }
}

// Function to test iput
void test_iput() {
    struct inode *in = ialloc();
    CTEST_ASSERT(in != NULL, "Should allocate an inode");

    iput(in);
    CTEST_ASSERT(in->ref_count == 0, "Reference count should be 0 after iput");
}

// Function to test iget
void test_iget() {
    reset_incore();

    // Allocate a new inode
    struct inode *new_inode = ialloc();
    CTEST_ASSERT(new_inode != NULL, "Should allocate an inode");

    // Use iget to get the same inode
    struct inode *fetched_inode = iget(new_inode->inode_num);
    CTEST_ASSERT(fetched_inode != NULL, "iget should return an in-core inode");
    CTEST_ASSERT(fetched_inode->inode_num == new_inode->inode_num, "Inode numbers should match");
    CTEST_ASSERT(fetched_inode->ref_count == 2, "Reference count should be 2");

    // Decrement the reference count
    iput(fetched_inode);
    CTEST_ASSERT(fetched_inode->ref_count == 1, "Reference count should be 1 after iput");

    // Decrement the reference count to 0
    iput(fetched_inode);
    CTEST_ASSERT(fetched_inode->ref_count == 0, "Reference count should be 0 after second iput");
}

// Function to test directory open
void test_directory_open() {
    struct directory *dir = directory_open(0);
    CTEST_ASSERT(dir != NULL, "directory_open: Success");
    if (dir != NULL) {
        directory_close(dir);
    } else {
        CTEST_ASSERT(0, "directory_open: Failed");
    }
}

// Function to test directory get
void test_directory_get() {
    struct directory *dir = directory_open(0);
    struct directory_entry ent;

    CTEST_ASSERT(dir != NULL, "directory_get: Open directory");

    if (dir == NULL) {
        printf("directory_get: Failed to open directory\n");
        return;
    }

    CTEST_ASSERT(directory_get(dir, &ent) == 0, "directory_get: First entry");
    CTEST_ASSERT(strcmp(ent.name, ".") == 0, "directory_get: First entry name is '.'");

    // Test reading the second entry
    CTEST_ASSERT(directory_get(dir, &ent) == 0, "directory_get: Second entry");
    CTEST_ASSERT(strcmp(ent.name, "..") == 0, "directory_get: Second entry name is '..'");

    // Test reading beyond available entries
    CTEST_ASSERT(directory_get(dir, &ent) == -1, "directory_get: End of directory");

    directory_close(dir);
}

// Function to test directory close
void test_directory_close() {
    struct directory *dir = directory_open(0);
    CTEST_ASSERT(dir != NULL, "directory_close: Open directory");
    if (dir == NULL) {
        printf("directory_close: Failed to open directory\n");
        return;
    }

    directory_close(dir);
    CTEST_ASSERT(1, "directory_close: Success");
}

// Function to test ls
void test_ls() {
    printf("Contents of the root directory:\n");
    ls();
}

// Function to test directory creation
void test_directory_make() {
    int result = directory_make("/foo");
    CTEST_ASSERT(result == 0, "directory_make: Create /foo directory");
    
    result = directory_make("/bar");
    CTEST_ASSERT(result == 0, "directory_make: Create /bar directory");

    // Try creating a directory in a non-root path (should fail based on assumptions)
    result = directory_make("/foo/bar");
    CTEST_ASSERT(result == -1, "directory_make: Fail to create /foo/bar directory");
}

// Function to test namei
void test_namei() {
    // Test retrieving the root inode
    struct inode *root_inode = namei("/");
    CTEST_ASSERT(root_inode != NULL, "namei: Get root inode");
    CTEST_ASSERT(root_inode->inode_num == ROOT_INODE_NUM, "namei: Root inode number should be ROOT_INODE_NUM");

    // Test retrieving a non-root inode (should fail for now)
    struct inode *nonexistent_inode = namei("/foo");
    CTEST_ASSERT(nonexistent_inode == NULL, "namei: Get /foo inode should fail for stub implementation");
}

int main() {
    // Initialize testing variables and configure testing environment
    CTEST_VERBOSE(1);
    CTEST_COLOR(1);

    // Open the image file first and test opening
    CTEST_ASSERT(image_open("test.img", 1) != -1, "Open image file with truncation");

    // Initialize inode and block maps after truncation
    unsigned char empty_map[BLOCK_SIZE] = {0};

    // Write initialized maps to inode map and block map
    bwrite(INODE_MAP_BLOCK, empty_map);
    bwrite(FREE_MAP_BLOCK, empty_map);

    // Initialize inode blocks
    for (int i = 0; i < NUM_INODE_BLOCKS; i++) {
        bwrite(FIRST_INODE_BLOCK + i, empty_map);
    }

    test_inode_allocation();

    int allocated_block = test_block_allocation();

    test_block_write_read(allocated_block);
    test_set_free_find_free(allocated_block);
    test_incore_functions();
    test_inode_read_write();
    test_iget();
    test_iput();

    // Close the initial image file
    CTEST_ASSERT(image_close() == 0, "Close initial image file");

    // Create the filesystem
    mkfs();

    // Test directory functions
    test_directory_open();
    test_directory_get();
    test_directory_close();
    test_ls();

    // Test directory creation and namei function
    test_directory_make();
    test_namei();

    // Close the fs image
    clfs();

    CTEST_RESULTS();

    CTEST_EXIT();
}
