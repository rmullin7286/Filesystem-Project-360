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

void touch()
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
    int original = running->fd[fd].offset;
    running->fd[fd].offset = (position <= running->fd[fd].mptr.inode.i_size && position >= 0) ? position : original;
    return original;
}

int write_file()
{
    int fd = atoi(pathname);
    if(running->fd[fd].refCount == 0)
    {
        printf("File descriptor not open\n");
        return;
    }
    if(running->fd[fd].mode == R)
    {
        printf("File is open for read\n");
        return;
    }

    mywrite(fd, pathname2, strlen(pathname2));
}

int mywrite(int fd, char buf[], int nbytes)
{
    int original_nbytes;
    OFT * oftp = &(running->fd[fd]);
    MINODE * mip = oftp->mptr;
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
            uint32_t buf[256];
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
            if(mip->i_block[13] == 0)
            {
                mip->i_block[13] = balloc(mip->dev);
                zero_block(mip->i_block, mip->i_block[13]);
            }
            get_block(mip->dev, mip->i_block[13], (char*)ibuf);
            if(buf[indirect1] == 0)
            {
                buf[indirect1] = balloc(mip->dev);
                put_block(mip->dev, mip->i_block[13], (char*)ibuf);
            }
            uint32_t ibuf2[256];
            get_block(mip->dev, buf[indirect1], (char*)ibuf2);
            if(buf2[indirect2] == 0)
            {
                buf2[indirect2] = balloc(mip->dev);
                put_block(mip->dev, buf[indirect1], (char*)ibuf2);
            }
            blk = ibuf2[indirect2];
        }

        char wbuf[BLKSIZE];
        get_block(mip->dev, blk, wbuf);
        char * cp = wbuf + startByte;
        char * cq = buf;
        int remain = BLKSIZE - startByte;

        while(remain > 0)
        {
            *cp++ = *cq++;
            nbytes--;
            remain--;
            oftp->offset++
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