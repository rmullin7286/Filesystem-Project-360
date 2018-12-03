#include "filesystem.h"
#include "util.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

Filesystem Filesystem_Init(const char * name)
{
    //open file
    const int dev = open(name, O_RDWR);
    if(dev < 0)
        return (Filesystem){.OK = false};

    Filesystem fs = {.dev = dev};

    //check magic number
    Super s = Filesystem_GetSuperBlock(&fs);
    if(s.s_magic != 0xEF53)
        return (Filesystem){.OK = false};
    fs.OK = true;

    //get nblocks, ninodes
    fs.nblocks = s.s_blocks_count;
    fs.ninodes = s.s_inodes_count;

    //get imap and bmap, inodes_start
    GD gd = Filesystem_GetGroupDescriptor(&fs);
    fs.imap = gd.bg_inode_bitmap;
    fs.bmap = gd.bg_block_bitmap;
    fs.inodes_start = gd.bg_inode_table;

    //get the root minode
    fs.root = Minode_Init(fs.dev, fs.inodes_start, MINODE_ROOT);

    //set all other Minodes to default empty Minode
    for(int i = 0; i < NUM_MINODES; i++)
        fs.minode[i] = EMPTY_MINODE;

    return fs;
}

void Filesystem_Close(Filesystem * fs)
{
    for(int i = 0; i < NUM_MINODES; i++)
        Minode_Close(&(fs->minode[i]));
}

Super Filesystem_GetSuperBlock(const Filesystem * fs)
{
    char buf[BLKSIZE];
    get_block(fs->dev, SUPERBLOCK, buf);
    return *((Super*)buf);
}

GD Filesystem_GetGroupDescriptor(const Filesystem * fs)
{
    char buf[BLKSIZE];
    get_block(fs->dev, GDBLOCK, buf);
    return *((GD*)buf);
}

Minode Minode_Init(int dev, int start, int ino)
{
    const int blk = (ino - 1) / 8 + start;
    const int offset = (ino - 1) % 8;
    char buf[BLKSIZE];
    get_block(dev, blk, buf);
    Inode * inode = ((Inode*) (buf + offset));
    return (Minode){.dev = dev, .block = blk, .offset = offset, .ino = ino, .inode = *inode, .refCount = 1, .dirty = false, .mounted = false};
}

void Minode_Close(Minode * m)
{
    m->refCount--;

    //write back to file
    if(m->refCount == 0 && m->dirty)
    {
        char buffer[BLKSIZE];
        get_block(m->dev, m->block, buffer);
        *((Inode*)buffer + m->offset) = m->inode;
        put_block(m->dev, m->block, buffer);
    }
    *m = EMPTY_MINODE;
}
