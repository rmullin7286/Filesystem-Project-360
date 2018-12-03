
/*********************************************************************
        You MAY use the util.o file to get a quick start.
 BUT you must write YOUR own util.c file in the final project
 The following are helps of how to wrtie functions in the util.c file
*********************************************************************/


/************** util.c file ****************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>

#include "type.h"

/**** globals defined in main.c file ****/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char gpath[128];
extern char *name[64];
extern int n;
extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256];


int get_block(int dev, int blk, char *buf)
{
    lseek(dev, blk*BLKSIZE, 0);
    int n = read(dev, buf, BLKSIZE);
    if (n<0) return 0;
    return 1;
}   

int put_block(int dev, int blk, char *buf)
{
    lseek(dev, blk*BLKSIZE, 0);
    if(write(dev, buf, BLKSIZE) != BLKSIZE)
        return 0;
    return 1;
}   

int tokenize(char *pathname)
{
  // tokenize pathname into n components: name[0] to name[n-1];
}

MINODE *iget(int dev, int ino)
{
    // return minode pointer to loaded INODE
    for(int i = 0; i < NMINODE; i++)
        if(minode[i].refCount > 0 && minode[i].dev == dev && minode[i].ino == ino)
        {
            minode[i].refCount++;
            return (minode + i);
        }

    // needed entry not in memory:
    int i;
    for(i = 0; i < NMINODE; i++)
        if(minode[i].refCount == 0)
        {
            minode[i].refCount = 1;
            minode[i].dev = dev;
            minode[i].ino = ino;
            break;
        }

    char buf[BLKSIZE];
    int blk = (ino - 1) / 8 + inode_start, offset = (ino - 1) % 8;
    get_block(dev, blk, buf);
    minode[i].inode = *((INODE*)buf + offset);
    return &(minode[i]);
}

int iput(MINODE *mip) // dispose a used minode by mip
{
    mip->refCount--;
    if (mip->refCount == 0 || mip->dirty)
    {
        char buf[BLKSIZE];
        int blk = (mip->ino - 1) / 8 + inode_start, offset = (mip->ino - 1) % 8;
        get_block(mip->dev, blk, buf);
        *((INODE*)buf + offset) = mip->inode;
        put_block(mip->dev, blk, buf);
    }
}

// serach a DIRectory INODE for entry with a given name
int search(MINODE *mip, char *name)
{
   // return ino if found; return 0 if NOT
}


// retrun inode number of pathname
int getino(char *pathname)
{ 
    MINODE * cur = (pathname[0] == '/') ? root : running->cwd;
    // SAME as LAB6 program: just return the pathname's ino;
    int size = tokenize(pathname);
    for(i = 0; i < size; i++)
    {
        int ino = search(cur, name[i]);
        if(ino == 0)
            return 0;
        cur = iget(cur->dev, ino);
    }
    return ino;
}

// THESE two functions are for pwd(running->cwd), which prints the absolute
// pathname of CWD. 
int findmyname(MINODE *parent, u32 myino, char *myname) 
{
    // parent -> at a DIR minode, find myname by myino
    // get name string of myino: SAME as search except by myino;
    // copy entry name (string) into myname[ ];
    char dbuf[BLKSIZE], temp[256];
    for(int i = 0; i < 12; i++)
    {
        if(parent->i_block[i] == 0)
            break;
        get_block(fd, ip->i_block[i], dbuf);
        char * cp = dbuf;
        DIR * dp = (DIR*)dbuf;

        while(cp < dbuf + BLKSIZE)
        {
            if(dp->inode == myino)
            {
                strncpy(myname, dp->name, dp->name_len);
                return 1;
            }
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }
    }
    return 0;
}


int findino(MINODE *mip, u32 *myino) 
{
    // fill myino with ino of . 
    // retrun ino of ..
}