
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
    char * temp;
    int i = 0;
    while(temp = strtok(pathname, "/") && i < 64)
    {
      name[i] = temp;
      i++;
    }
    return i;
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
  char dbuf[BLKSIZE], temp[256];
  DIR *dp;
  char * cp;
  for (int i = 0; i < 12; i++)
  {
    if (ip->i_block[i] == 0)
      break;
    get_block(fd, ip->i_block[i], dbuf);
    dp = (DIR*) dbuf;
    cp = dbuf;

    while (cp < dbuf + BLKSIZE)
    {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;
      
      if(strncmp(name,dp->name, dp->name_len) == 0)
      {
        return dp->inode;
      }
      cp += dp->rec_len;
      dp = (DIR*)cp;
    }
  }
  return 0;
   // return ino if found; return 0 if NOT
}


// retrun inode number of pathname
int getino(char *pathname)
{ 
    MINODE * cur = (pathname[0] == '/') ? root : running->cwd;
    // SAME as LAB6 program: just return the pathname's ino;
    int size = tokenize(pathname);
    int ino;
    for(int i = 0; i < size; i++)
    {
        ino = search(cur, name[i]);
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

int tst_bit(char * buf, int bit)
{
    return buf[i] & ((bit/8) << (bit%8));
}

void set_bit(char * buf, int bit)
{
    buf[bit/8] |= (1 << (bit%8));
}

void clr_bit(char * buf, int bit)
{
    buf[bit/8] &= ~(1 << (bit%8));
}

int decFreeInodes(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, 1, buf);
    ((SUPER*)buf)->s_free_inodes_count--;
    put_block(dev, 1, buf);

    get_block(dev, 2, buf);
    ((GD*)buf)->bg_free_inodes_count--;
    put_block(dev, 2, buf);
}

int ialloc(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, imap, buf);
    for(int i = 0; i < ninodes; i++)
        if(tst_bit(buf, i) == 0)
        {
            set_bit(buf, i);
            decFreeInodes(dev);
            put_block(dev, imap, buf);
            return i + 1;
        }
    return 0;
}

int balloc(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, bmap, buf);
    for(int i = 0; i < nblocks; i++)
        if(tst_bit(buf, i) == 0)
        {
            set_bit(buf, i);
            decFreeInodes(dev);
            put_block(dev, bmap, buf);
            return i + 1;
        }
    return 0;
}
