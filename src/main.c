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

void mount_root(char * name)  // mount root file system, establish / and CWDs
{
    dev = open(name, O_RDWR);
    char buf[1024];
    get_block(dev, 1, buf);
    SUPER* s = (SUPER*)buf;
    if(s->s_magic != 0xEF53)
    {
        printf("NOT AN EXT2 FILESYSTEM!\n");
        exit(0);
    }
    nblocks = s->s_blocks_count;
    ninodes = s->s_inodes_count;

    get_block(dev, 2, buf);
    GD * g = (GD*)buf;
    bmap = g->bg_block_bitmap;
    imap = g->bg_inode_bitmap;
    inodes_start = gp->bg_inode_table;

    proc[0].cwd = proc[1].cwd = iget(dev, 2);
}

//HOW TO chdir(char *pathname)
void mychdir(char *pathname)
{
    if (strlen(pathname) == 0 || strcmp(pathname, "/") == 0)
        running->cwd = root;
    else
    {
        int ino = getino(pathname);
        MINODE *mip = iget(dev, ino);
        if(!(S_ISDIR(mip->inode.st_mode)))
        {
            printf("%s is not a directory", pathname);
            return;
        }
        iput(running->cwd);
        running->cwd = mip;
    }
}

void ls_dir(char * dirname)
{
    int ino = getino(dirname);
    MINODE * mip = iget(dev, ino);
    char buf[BLKSIZE];
    for(int i = 0; i < 12; i++)
    {
        if(mip->inode.i_block[i] == 0)
            break;
        get_block(dev, mip->inode.i_block[i], buf);
        char * cp = buf;
        DIR * dp = (DIR*)buf;

        while(cp < BLKSIZE + buf)
        {
            ls_file(dp->inode);
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }
    }
    iput(mip);
}

void ls_file(int ino)
{
    MINODE * mip = iget(dev, ino);
    const char * t1 = "xwrxwrxwr-------";
    const char * t2 = "----------------";
    if(S_ISREG(mip->i_mode))
        putchar('-')
    else if(S_ISDIR(mip->i_mode))
        putchar('d')
    else if(S_ISLNK(mip->i_mode))
        putchar('l');
    for(int i = 8; i >= 0; i--)
        putchar(((mip->i_mode) & (1 << i)) ? t1[i] : t2[i]);
    printf(" %4d ", mip->i_links_count);
    printf("%4d ", mip->i_gid);
    printf("%4d ", mip->i_uid);
    printf("%4d ", mip->i_size);
    printf("%s ", ctime(mip->i_ctime))

    MINODE * parent = iget(dev, search(mip, ".."));
    char buf[256];
    findmyname(parent, ino, buf);
    printf("%s", basename(buf));
    iput(parent);

    //TODO: IMPLEMENT PRINTING LINK
    putchar('\n');
    
    iput(mip);   
}



void pwd(MINODE *wd)
{
    if(wd == root)
    {
        puts("/");
        return;
    }
    rpwd(wd);
    // if (wd == root) print "/"
    // else
    // rpwd(wd);
}

void rpwd(MINODE *wd)
{
    MINODE * pip;
    char buf[256];
    int parent_ino, my_ino;
    if (wd==root)
        return;
    parent_ino = search(wd,"..");
    my_ino = search(wd,".");
    pip = iget(dev, search(wd, ".."));
    findmyname(pip, my_ino, buf);
    rpwd(pip);
    print("/%s", buf);
}

void quit()
{
    // iput() all minodes with (refCount > 0 && DIRTY)
    for (int i = 0; i < NMINODE; i++)
    {
        MINODE * mip = &minode[i];
        if (mip->refCount > 0 && mip->dirty == 1)
        {
            mip->refCount = 1;
            iput(mip);
        }
    }
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

