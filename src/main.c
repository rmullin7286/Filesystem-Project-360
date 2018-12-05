/*********************************************************
* COPYRIGHT xXRIS3NGAM3RZ420Xx LLC, 2018
**********************************************************/
#include <stdio.h>
#include <fcntl.h>

#include <string.h>
#include <stdlib.h>

#include "level2.h"
#include "level1.h"
#include "type.h"
#include "util.h"

MINODE minode[NMINODE];
MINODE *root;
PROC proc[NPROC], *running;
OFT oft[NOFT];

char gpath[256];
char *name[64]; // assume at most 64 components in pathnames
int  n;

int  fd, dev;
int  nblocks, ninodes, bmap, imap, inode_start;
char line[256], cmd[32], pathname[256], pathname2[256], dname[256], bname[256];
int data_start;

void init(void) // Initialize data structures of LEVEL-1:
{
    proc[0].uid = 0;
    proc[1].uid = 1;
    running = proc;
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
    data_start = s->s_first_data_block;
    nblocks = s->s_blocks_count;
    ninodes = s->s_inodes_count;

    get_block(dev, 2, buf);
    GD * g = (GD*)buf;
    bmap = g->bg_block_bitmap;
    imap = g->bg_inode_bitmap;
    inode_start = g->bg_inode_table;

    root = iget(dev, 2);
    proc[0].cwd = iget(dev, 2);
    proc[1].cwd = iget(dev, 2);
}

void quit()
{
    // iput() all minodes with (refCount > 0 && DIRTY)
    for (int i = 0; i < NMINODE; i++)
        if (minode[i].refCount > 0 && minode[i].dirty == 1)
        {
            minode[i].refCount = 1;
            iput(&(minode[i]));
        }
    exit(0); 
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        printf("Usage: fs360 diskname\n");
        exit(1);
    }
    init();
    mount_root(argv[1]);

    void (*fptr[]) () = { (void (*)())makedir, rmdir,mychdir,ls,pwd,create_file,mystat,link,symlink,unlink, mychmod, myopen, read_file, write_file, myclose, cp, mycat, quit};
    char *cmdNames[18] = {"mkdir", "rmdir", "cd", "ls", "pwd", "creat", "stat", "link", "symlink", "unlink", "chmod", "open", "read", "write", "close", "cp", "cat", "quit"};

    while(1)
    {
        cmd[0] = pathname[0] = pathname2[0] = '\0';
        int i;
        printf("\033[1;34mbdesh\033[0m $");
        fgets(line,256,stdin);
        sscanf(line, "%s %s %s", cmd, pathname, pathname2);
        for(i = 0;(strcmp(cmd,cmdNames[i])) && i < 18; i++);
        if(i != 18)
        {
            fptr[i]();
        }
        else
        {
            printf("%s is not a valid function. Use command \"help\" to see what commands are available.\n", cmd);
        }
    }
}

