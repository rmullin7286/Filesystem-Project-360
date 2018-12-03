#include "util.h"

#include <unistd.h>
#include <stdint.h>

void get_block(int fd, int blk, char buf[ ])
{
  lseek(fd, blk*BLKSIZE, 0);
  read(fd, buf, BLKSIZE);
}

void put_block(int fd, int blk, char buf[])
{
    lseek(fd, blk*BLKSIZE, 0);
    write(fd, buf, BLKSIZE);
}

void read_indirect_block(int fd, int blk, int indirect, char buf[])
{
    uint32_t ibuf[INDIRECT_BLOCK_LENGTH];
    get_block(fd, blk, (char*)ibuf);
    blk = ibuf[indirect];
    get_block(fd, blk, buf);
    
}

void read_double_indirect(int fd, int blk, int indirect, int indirect2, char buf[])
{
    uint32_t ibuf[INDIRECT_BLOCK_LENGTH];
    get_block(fd, blk, (char*)ibuf);
    blk = ibuf[indirect];
    get_block(fd, blk, (char*)ibuf);
    blk = ibuf[indirect2];
    get_block(fd, blk, buf);
}