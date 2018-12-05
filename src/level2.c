#include "type.h"
#include "util.c"

void myopen()
{
    int ino = getino(pathname);
    int mode = atoi(pathname2);
    if(mode < 0 || mode > 3)
    {
        printf("Invalid mode\n");
        return;
    }
    if(!ino)
    {
        printf("File does not exist\n");
        return;
    }
    
    MINODE * mip = iget(dev, ino);
    if(!S_ISREG(mip->inode.i_mode))
    {
        printf("Not a regular file\n");
        return;
    }

    for(int i = 0; i < NOFT; i++)
        if(oft[i].mptr == mip && oft[i].mode != R)
        {
            printf("File already open\n");
            return;
        }

    for(int i = 0; i < NOFT; i++)
        if(oft[i].refCount == 0)
        {
            oft[i] = (OFT){.mode = mode, .refCount = 1, .mptr = mip, .offset = (mode == APPEND ? mip->inode.i_size : 0)};
            for(int j = 0; j < NFD; j++)
                if(!running->fd[j])
                {
                    running->fd[j] = (oft + i);
                    break;
                }
            time_t current_time = time(0L);
            mip->inode.i_atime = current_time;
            if(mode != R)
                mip->inode.i_mtime = current_time;
            mip->dirty = 1;
            printf("File opened at file descriptor %d\n", i);
            break;
        }
}

void myclose(int fd)
{
    if(!(0<=fd<10) || running->fd[fd] == 0)
    {
        printf("ERROR: Invalid fd.\n");
        return;
    }

    OFT *op = running->fd[fd];
    op->refCount--;
    if(op->refCount == 0)
        iput(op->mptr);

    running->fd[fd] = 0;
}

int myread(int fd, char *buf, int nbytes)
{
    MINODE *mip = running->fd[fd]->mptr;
    OFT *op = running->fd[fd];
    int count = 0, avil = mip->inode.i_size - op->offset;
    char *cq = buf, dbuf[BLKSIZE];
    int blk;

    while(nbytes > 0 && avil > 0)
    {
        int lbk = op->offset / BLKSIZE, startByte = op->offset % BLKSIZE;

        if (lbk < 12)
        {
            blk = mip->inode.i_block[lbk];
        }
        else if(lbk >= 12 && lbk < 268)
        {
            get_block(mip->dev,mip->inode.i_block[12], dbuf);
            char *cp = dbuf;
            blk = cp[lbk -12];
        }
        else
        {
            get_block(mip->dev,mip->inode.i_block[13], dbuf); // Get to indirect blocks from double indirect
            cq = dbuf + ((lbk - 268) / 256); // Find which indirect block to go to
            get_block(mip->dev, *((int*)cq), dbuf); // Get to indirect block
            cq = dbuf + ((lbk -268) % 256); // Go to direct block from indirect block
            blk = *((int*)cq);// (int) *cq? Save direct block value to blk
        }

        // Get data block into readbuf
        char readbuf[BLKSIZE];
        get_block(mip->dev,blk, readbuf);

        // Copy from startByte to buf[], at most remain bytes in the block
        cq = readbuf + startByte;
        int remainingBytes = BLKSIZE - startByte;
        if(nbytes < remainingBytes)
        {
            remainingBytes = nbytes;
        }
        memcpy((buf + count), cq, remainingBytes);
        op->offset += remainingBytes;
        count += remainingBytes;
        avil -= remainingBytes;
        nbytes -= remainingBytes;
        if (nbytes <= 0 || avil <= 0)
            break;
    }
    printf("myread: read %d char from file descriptor %d.\n", count, fd);
    return count;
}

int read_file(int fd, int nbytes)
{
    char buf[BLKSIZE];
    if(running->fd[fd] == 0 || (running->fd[fd]->mode != R && running->fd[fd]->mode != RW))
    {
        printf("ERROR: FD %d is not open for read.\n", fd);
        return -1;
    }
    return (myread(fd, buf, nbytes));

}