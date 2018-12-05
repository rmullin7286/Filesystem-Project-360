#include "type.h"
#include "util.c"

void myopen()
{
    int ino = getino(pathname);
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

void myclose()
{
    int fd = atoi(pathname);
    if(!(0<=fd && fd <10) || running->fd[fd] == 0)
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

int read_file()
{
    int fd = atoi(pathname), nbytes = atoi(pathname2);
    char buf[nbytes + 1];
    if(running->fd[fd] == 0 || (running->fd[fd]->mode != R && running->fd[fd]->mode != RW))
    {
        printf("ERROR: FD %d is not open for read.\n", fd);
        return -1;
    }
    int ret = myread(fd, buf, nbytes);
    buf[ret] = 0;
    printf("%s", buf);
    return ret;
}

void mytouch()
{
    char * pathdup = strdup(pathname);
    int ino = getino(pathdup);
    free(pathdup);
    if(!ino)
        create_file();
    else
    {
        MINODE * mip = iget(dev, ino);
        mip->inode.i_atime = mip->inode.i_mtime = time(0L);
        mip->dirty = 1;
        iput(mip);
    }
}

int mylseek(int fd, int position)
{
    int original = running->fd[fd]->offset;
    running->fd[fd]->offset = (position <= running->fd[fd]->mptr->inode.i_size && position >= 0) ? position : original;
    return original;
}

int write_file()
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
        else if(lbk == 12 && lbk < 256 + 12)
        {
            if(mip->inode.i_block[12] == 0)
            {
                mip->inode.i_block[12] = balloc(mip->dev);
                zero_block(mip->dev, mip->inode.i_block[12]);
            }
            uint32_t ibuf[256];
            get_block(mip->dev, mip->inode.i_block[12], (char*)buf);
            blk = ibuf[lbk - 12];
            if(blk == 0)
            {
                blk = ibuf[lbk - 12] = balloc(mip->dev);
                put_block(mip->dev, mip->inode.i_block[12], (char*)buf);
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
                zero_block(mip->inode.i_block, mip->inode.i_block[13]);
            }
            get_block(mip->dev, mip->inode.i_block[13], (char*)ibuf);
            if(buf[indirect1] == 0)
            {
                buf[indirect1] = balloc(mip->dev);
                put_block(mip->dev, mip->inode.i_block[13], (char*)ibuf);
            }
            uint32_t ibuf2[256];
            get_block(mip->dev, buf[indirect1], (char*)ibuf2);
            if(ibuf2[indirect2] == 0)
            {
                ibuf2[indirect2] = balloc(mip->dev);
                put_block(mip->dev, buf[indirect1], (char*)ibuf2);
            }
            blk = ibuf2[indirect2];
        }

        char wbuf[BLKSIZE];
        get_block(mip->dev, blk, wbuf);
        char * cp = wbuf + startByte;
        int remain = BLKSIZE - startByte;

        while(remain > 0)
        {
            *cp++ = *cq++;
            nbytes--;
            remain--;
            oftp->offset++;
            if(oftp->offset > mip->inode.i_size)
                mip->inode.i_size++;
            if(nbytes <= 0)
                break;
        }
        put_block(mip->dev, blk, wbuf);
    }

    mip->dirty = 1;
    printf("wrote %d char into the file descriptor fd=%d\n", original_nbytes, fd);
    return original_nbytes;

    //TODO: OPTMIZE
}