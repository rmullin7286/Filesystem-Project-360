#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>

#include "type.h"

#include "util.c"

MINODE minode[NMINODE];
MINODE *root;
PROC proc[NPROC], *running;

char gpath[256];
char *name[64]; // assume at most 64 components in pathnames
int  n;

int  fd, dev;
int  nblocks, ninodes, bmap, imap, inode_start;
char line[256], cmd[32], pathname[256];

void init(void) // Initialize data structures of LEVEL-1:
{
    proc[0].uid = 0;
    proc[1].uid = 1;
    proc[0].cwd = proc[1].cwd = root;
    running = proc;
    root = NULL;
    for(int i = 0; i < NMINODE; i++)
        minode[i].refCount = 0;
}

void mount_root(void)  // mount root file system, establish / and CWDs
{
    /*
      open device for RW (get a file descriptor as dev for the opened device)
      read SUPER block to verify it's an EXT2 FS
      record nblocks, ninodes as globals

      read GD0; record bamp, imap, inodes_start as globals;
      
      root = iget(dev, 2);
   
      Let cwd of both P0 and P1 point at the root minode (refCount=3)
          P0.cwd = iget(dev, 2); 
          P1.cwd = iget(dev, 2);

      Let running -> P0.
      */
}

HOW TO chdir(char *pathname)
   {
      if (no pathname)
         cd to root;
      else
         cd to pathname by
      (1).  ino = getino(pathname);
      (2).  mip = iget(dev, ino);
      (3).  verify mip->INODE is a DIR
      (4).  iput(running->cwd);
      (5).  running->cwd = mip;
}

void ls_dir(char * dirname)
{
      
      ino = getino(dirname);
      mip = iget(dev, ino); ===> mip points at dirname's minode
                                                         INODE  
                                                         other fields
      Get INODE.i_block[0] into a buf[ ];
      Step through each dir_entry (skip . and .. if you want)
      For each dir_entry: you have its ino.
      call ls_file(ino)
}

void ls_file(int ino)
{

      mip = iget(dev, ino); ==> mip points at minode
                                              INODE
      Use INODE contents to ls it 
}



void pwd(MINODE *wd)
{
    if (wd == root) print "/"
    else
    rpwd(wd);
}

void rpwd(MINODE *wd)
{
    if (wd==root) return;
    from i_block[0] of wd->INODE: get my_ino of . parent_ino of ..
    pip = iget(dev, parent_ino);
    from pip->INODE.i_block[0]: get my_name string as LOCAL

    rpwd(pip);  // recursive call rpwd() with pip

    print "/%s", my_name;
}

void quit()
{
    iput() all minodes with (refCount > 0 && DIRTY);
    exit(0); 
}

/*
int main(void)
{
    init();
    mount_root();

    while(1){
        //  ask for a command line = "cmd [pathname]"
        //  cmd=ls:
        ls(pathname);
        //  cmd=cd:
        chdir(pathname);
        //  cmd=pwd
        pwd(running->cwd)
        cmd=quit
        quit();
}
*/

