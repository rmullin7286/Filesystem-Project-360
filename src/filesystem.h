/*
* This file declares wrapper structs and functions
* for interacting with the EXT2 Filesystem
*/

#pragma once

#include "error.h"

#include <stdbool.h>
#include <ext2fs/ext2_fs.h>

typedef struct ext2_inode Inode;
typedef struct ext2_super_block Super;
typedef struct ext2_group_desc GD;
typedef struct ext2_dir_entry_2 _entry;

#define NUM_MINODES 64
#define MINODE_ROOT 2

//these are the max pathname and filename lengths for ext2 filesystems
#define EXT2_MAX_FILENAME 255
#define EXT2_MAX_PATHNAME 4096
#define EXT2_NUM_BLOCKS 15

//A wrapper struct around an inode with extra values and functionality
typedef struct minode
{
    int dev;
    int block;
    int offset;
    int ino;
    Inode inode;
    int refCount;
    bool dirty;
    bool mounted;
}Minode;

//loads a Minode from device of id dev
Minode Minode_Init(int dev, int start, int ino);

/*
* writes a Minode back to file if dirty and refCount reaches 0
* sets Minode to EMPTY_MINODE
*/
void Minode_Close(Minode*);

//This macro defines the default empty state of a Minode that hasn't been loaded into yet
#define EMPTY_MINODE ((Minode){.dev = -1, .ino = 0, .refCount = 0, .dirty = false, .mounted = false})



//This struct represents the entire EXT2 filesystem
typedef struct filesystem
{
    int dev;
    bool OK;
    int nblocks;
    int ninodes;
    int imap;
    int bmap;
    int inodes_start;
    Minode root;
    Minode minode[NUM_MINODES];
}Filesystem;

/*
* Loads a Filesystem from the disk located at the path specified by name
* Returns a filesystem. Filesystem.OK will be set to false if the filesystem could not
* be loaded. Otherwise it will be set to true, and all other values should be set correctly.
*/
Filesystem Filesystem_Init(const char * name);

/*
* Closes the Filesystem and writes all Minodes back to memory.
*/
void Filesystem_Close(Filesystem*);

Super Filesystem_GetSuperBlock(const Filesystem*);
GD Filesystem_GetGroupDescriptor(const Filesystem*);
Minode Filesystem_SearchPath(Filesystem*, const char * path);
