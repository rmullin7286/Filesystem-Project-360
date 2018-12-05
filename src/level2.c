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
