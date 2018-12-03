/*
* This file defines useful function to use
* in other parts of the program
*/

#pragma once

#define BLKSIZE 1024
#define SUPERBLOCK 1
#define GDBLOCK 2
#define INDIRECT_BLOCK_LENGTH 256

void get_block(int fd, int blk, char buf[]);
void put_block(int fd, int blk, char buf[]);

void read_indirect_block(int fd, int blk, int indirect, char buf[]);
void read_double_indirect(int fd, int blk, int indirect, int indirect2, char buf[]);