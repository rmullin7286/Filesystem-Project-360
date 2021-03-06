#include "level2.h"
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "globals.h"
#include "level1.h"

void myopen()
{
    int mode;
    if(strcmp(pathname2, "R") == 0)
        mode = R;
    else if(strcmp(pathname2, "W") == 0)
        mode = W;
    else if(strcmp(pathname2, "RW") == 0)
        mode = RW;
    else if(strcmp(pathname2, "APPEND") == 0)
        mode = APPEND;
    else
        mode = -1;

    int fd = open_file(pathname, mode);
    if(fd >= 0 && fd < 10)
        printf("Opened file descriptor at %d\n", fd);
}

int open_file(char * name, int mode)
{
    int ino = getino(name);

    if(mode < 0 || mode > 3)
    {
        printf("Invalid mode\n");
        return -1;
    }
    if(!ino)
    {
        printf("File does not exist\n");
        return -1;
    }
    
    MINODE * mip = iget(dev, ino);
    if(!S_ISREG(mip->inode.i_mode))
    {
        printf("Not a regular file\n");
        return -1;
    }

    for(int i = 0; i < NOFT; i++)
        if(oft[i].mptr == mip && oft[i].mode != R)
        {
            printf("File already open\n");
            return -1;
        }

    for(int i = 0; i < NOFT; i++)
        if(oft[i].refCount == 0)
        {
            oft[i] = (OFT){.mode = mode, .refCount = 1, .mptr = mip, .offset = (mode == APPEND ? mip->inode.i_size : 0)};
            if(mode == W)
                mip->inode.i_size = 0;
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
            return i;
        }
    return -1;
}

void myclose()
{
    int fd = atoi(pathname);
    if(!(0<=fd && fd <10) || running->fd[fd] == 0)
    {
        printf("ERROR: Invalid fd.\n");
        return;
    }

    close_file(fd);
}

void close_file(int fd)
{
    OFT *op = running->fd[fd];
    op->refCount--;
    if(op->refCount == 0)
        iput(op->mptr);

    oft[fd].mptr = NULL;
    running->fd[fd] = NULL;
}

int myread(int fd, char *buf, int nbytes)
{
    MINODE *mip = running->fd[fd]->mptr;
    OFT *op = running->fd[fd];
    int count = 0, avil = mip->inode.i_size - op->offset;
    char *cq = buf;
    uint32_t dbuf[256];
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
            get_block(mip->dev,mip->inode.i_block[12], (char*)dbuf);// Get to blocks from indirect blocks
            blk = dbuf[lbk - 12];
        }
        else
        {
            get_block(mip->dev,mip->inode.i_block[13], (char*)dbuf); // Get to indirect blocks from double indirect
            //cq = dbuf + ((lbk - 268) / 256); // Find which indirect block to go to
            get_block(mip->dev, dbuf[(lbk - 268) / 256], (char*)dbuf); // Get to indirect block
            //cq = dbuf + ((lbk -268) % 256); // Go to direct block from indirect block
            blk = dbuf[(lbk-268) % 256];// (int) *cq? Save direct block value to blk
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

void read_file()
{
    int fd = atoi(pathname), nbytes = atoi(pathname2);
    char buf[nbytes + 1];
    if(running->fd[fd] == 0 || (running->fd[fd]->mode != R && running->fd[fd]->mode != RW))
    {
        printf("ERROR: FD %d is not open for read.\n", fd);
        return;
    }
    int ret = myread(fd, buf, nbytes);
    buf[ret] = 0;
    printf("%s\n", buf);
}

void mytouch()
{
    char * pathdup = strdup(pathname);
    touch_file(pathdup);
    free(pathdup);
}


void touch_file(char * name)
{
    int ino = getino(name);
    if(!ino)
        make_entry(0, name);
    else
    {
        MINODE * mip = iget(dev, ino);
        mip->inode.i_atime = mip->inode.i_mtime = time(0L);
        mip->dirty = 1;
        iput(mip);
    }
}

void mylseek(int fd, int position)
{
    int original = running->fd[fd]->offset;
    running->fd[fd]->offset = (position <= running->fd[fd]->mptr->inode.i_size && position >= 0) ? position : original;
    return;
}

void write_file()
{
    int fd = atoi(pathname);
    if(running->fd[fd]->refCount == 0)
    {
        printf("File descriptor not open\n");
        return;
    }
    if(running->fd[fd]->mode == R)
    {
        printf("File is open for read\n");
        return;
    }

    mywrite(fd, pathname2, strlen(pathname2));
}

int mywrite(int fd, char buf[], int nbytes)
{
    int original_nbytes = nbytes;
    OFT * oftp = (running->fd[fd]);
    MINODE * mip = oftp->mptr;
    char * cq = buf;
    while(nbytes > 0)
    {
        int lbk = oftp->offset / BLKSIZE, startByte = oftp->offset % BLKSIZE, blk;

        if(lbk < 12)
        {
            if(mip->inode.i_block[lbk] == 0)
                mip->inode.i_block[lbk] = balloc(mip->dev);
            blk = mip->inode.i_block[lbk];
        }
        else if(lbk >= 12 && lbk < 256 + 12)
        {
            if(mip->inode.i_block[12] == 0)
            {
                mip->inode.i_block[12] = balloc(mip->dev);
                zero_block(mip->dev, mip->inode.i_block[12]);
            }
            uint32_t ibuf[256];
            get_block(mip->dev, mip->inode.i_block[12], (char*)ibuf);
            blk = ibuf[lbk - 12];
            if(blk == 0)
            {
                if((blk = ibuf[lbk - 12] = balloc(mip->dev)) == 0)
                {
                    printf("Ran out of disk space!!!!!!!!\n");
                    return original_nbytes - nbytes;
                }
                put_block(mip->dev, mip->inode.i_block[12], (char*)ibuf);
            }
        }
        else
        {
            int indirect1 = (lbk - 256 - 12) / 256;
            int indirect2 = (lbk - 256 - 12) % 256;
            uint32_t ibuf[256];
            if(mip->inode.i_block[13] == 0)
            {
                mip->inode.i_block[13] = balloc(mip->dev);
                zero_block(mip->inode.i_block[13], mip->inode.i_block[13]);
            }
            get_block(mip->dev, mip->inode.i_block[13], (char*)ibuf);
            if(ibuf[indirect1] == 0)
            {
                ibuf[indirect1] = balloc(mip->dev);
                zero_block(mip->dev, ibuf[indirect1]);
                put_block(mip->dev, mip->inode.i_block[13], (char*)ibuf);
            }
            uint32_t ibuf2[256];
            get_block(mip->dev, ibuf[indirect1], (char*)ibuf2);
            if(ibuf2[indirect2] == 0)
            {
                ibuf2[indirect2] = balloc(mip->dev);
                zero_block(mip->dev, ibuf2[indirect2]);
                put_block(mip->dev, ibuf[indirect1], (char*)ibuf2);
            }
            blk = ibuf2[indirect2];
        }

        char wbuf[BLKSIZE];
        zero_block(mip->dev, blk);
        get_block(mip->dev, blk, wbuf);
        char * cp = wbuf + startByte;
        int remain = BLKSIZE - startByte;
        if(nbytes < remain)
            remain = nbytes;
        memcpy(cp, cq, remain);
        cq += remain;
        oftp->offset += remain;
        nbytes -= remain;
        mip->inode.i_size += remain;
        put_block(mip->dev, blk, wbuf);
        // Unoptimized
        // while(remain > 0)
        // {
        //     *cp++ = *cq++;
        //     nbytes--;
        //     remain--;
        //     oftp->offset++;
        //     if(oftp->offset > mip->inode.i_size)
        //         mip->inode.i_size++;
        //     if(nbytes <= 0)
        //         break;
        // }
    }

    mip->dirty = 1;
    printf("wrote %d char into the file descriptor fd=%d\n", original_nbytes, fd);
    return original_nbytes;

    //TODO: OPTMIZE
}

void cp()
{
    int fdsource = open_file(pathname, R);
    char temp[strlen(pathname2) + 1];
    strcpy(temp, pathname2);
    touch_file(temp);
    int fddest = open_file(pathname2, W);
    char * buffer = malloc(sizeof(char) * running->fd[fdsource]->mptr->inode.i_size + 1);
    myread(fdsource, buffer, running->fd[fdsource]->mptr->inode.i_size);
    buffer[running->fd[fdsource]->mptr->inode.i_size] = 0;
    mywrite(fddest, buffer, running->fd[fdsource]->mptr->inode.i_size);
    free(buffer);
    close_file(fdsource);
    close_file(fddest);
}

void mycat()
{
    int fd = open_file(pathname, R);
    if(fd == -1)
    {
        return;
    }
    char *buf = malloc(sizeof(char) * (running->fd[fd]->mptr->inode.i_size + 1));
    myread(fd,buf,running->fd[fd]->mptr->inode.i_size);
    buf[running->fd[fd]->mptr->inode.i_size] = 0;
    printf("%s\n", buf);
    free(buf);
    close_file(fd);
}

void mymov()
{
    char * path1temp = strdup(pathname);
    link();
    strcpy(pathname, path1temp);
    unlink();
    free(path1temp);
}