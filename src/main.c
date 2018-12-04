/*********************************************************
* COPYRIGHT xXRISENGAMERZXx LLC, 2018
**********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

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
char line[256], cmd[32], pathname[256], pathname2[256], dname[256], bname[256];

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
    nblocks = s->s_blocks_count;
    ninodes = s->s_inodes_count;

    get_block(dev, 2, buf);
    GD * g = (GD*)buf;
    bmap = g->bg_block_bitmap;
    imap = g->bg_inode_bitmap;
    inode_start = g->bg_inode_table;

    proc[0].cwd = proc[1].cwd = iget(dev, 2);
}

//HOW TO chdir(char *pathname)
void mychdir()
{
    if (strlen(pathname) == 0 || strcmp(pathname, "/") == 0)
        running->cwd = root;
    else
    {
        int ino = getino(pathname);
        MINODE *mip = iget(dev, ino);
        if(!(S_ISDIR(mip->inode.i_mode)))
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
    if(S_ISREG(mip->inode.i_mode))
        putchar('-');
    else if(S_ISDIR(mip->inode.i_mode))
        putchar('d');
    else if(S_ISLNK(mip->inode.i_mode))
        putchar('l');
    for(int i = 8; i >= 0; i--)
        putchar(((mip->inode.i_mode) & (1 << i)) ? t1[i] : t2[i]);

    printf(" %4d %4d %4d %4d %s ", mip->inode.i_links_count, mip->inode.i_gid, mip->inode.i_uid, mip->inode.i_size,
            ctime(mip->inode.i_ctime));

    MINODE * parent = iget(dev, search(mip, ".."));
    char buf[256];
    findmyname(parent, ino, buf);
    printf("%s\n", basename(buf));
    iput(parent);

    //TODO: IMPLEMENT PRINTING LINK

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
    char buf[256];
    if (wd==root)
        return;
    int parent_ino = search(wd,"..");
    int my_ino = search(wd,".");
    MINODE * pip = iget(dev, search(wd, ".."));
    findmyname(pip, my_ino, buf);
    rpwd(pip);
    iput(pip);
    printf("/%s", buf);
}

int ideal_length(int name_len)
{
    return 4 * ((11 + name_len) / 4);
}

void enter_name(MINODE * pip, int myino, char * myname)
{
    int need_len = ideal_length(strlen(myname));
    for(int i = 0; i < 12; i++)
    {
        char buf[BLKSIZE];
        DIR * dp = (DIR*)buf;
        char * cp = buf;

        if(pip->inode.i_block[i] == 0)
        {
            //allocate a new block
            pip->inode.i_block[i] = balloc(dev);
            get_block(pip->dev, pip->inode.i_block[i], buf);
            *dp = (DIR){.inode = myino, .rec_len = BLKSIZE, .name_len = strlen(myname)};
            strncpy(dp->name, myname, dp->name_len);
        }
 
        get_block(pip->dev, pip->inode.i_block[i], buf);

        //get to the last entry in the block
        while(cp + dp->rec_len < buf + BLKSIZE)
        {
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }

        if(dp->rec_len - ideal_length(dp->name_len) >= need_len)
        {
            //put entry in existing block
            dp->rec_len = ideal_length(dp->name_len);
            dp = ((DIR*)cp + dp->rec_len);
            *dp = (DIR){.inode = myino, .rec_len = BLKSIZE - (cp + dp->rec_len), .name_len = strlen(myname)};
            strncpy(dp->name, myname, dp->name_len);
            put_block(pip->dev, pip->inode.i_block[i], buf);
            return;
        }
    }
}

void make_entry(int dir)
{
    char * base = basename(pathname), * dir = dirname(pathname);
    int parent = getino(dirname);
    if(!parent)
    {
        printf("ERROR: Specified path does not exist\n");
        return;
    }
    MINODE * pip = iget(dev, parent);
    if(!S_ISDIR(pip->inode.i_mode))
    {
        printf("ERROR: Specified path is not a directory\n");
        return;
    }
    if(search(pip, base))
    {
        printf("ERROR: Entry %s already exists\n", base);
        return;
    }

    if(dir)
        pip->inode.i_links_count++;
    
    int ino = ialloc(dev), bno = dir ? balloc(dev) : 0;

    MINODE * mip = iget(dev, ino);
    INODE * ip = &(mip->inode);
    time_t t = time(0L);
    *ip = (INODE){.i_mode = (dir ? 0x41ED : (S_IFREG | 0644)), .i_uid = running->uid, .i_gid = running->gid, .i_size = BLKSIZE, .i_links_count = (dir ? 2 : 1),
            .i_atime = t, .i_ctime = t, .i_mtime = t, .i_blocks = (dir ? 2 : 0), .i_block = {bno}};
    mip->dirty = 1;

    if(dir)
    {
        char buf[BLKSIZE] = {0};
        *((DIR*)buf) = (DIR){.inode = ino, .rec_len = 12, .name_len = 1, .name = "."};
        *((DIR*)buf +  12) = (DIR){.inode = ino, .rec_len = 12, .name_len = 2, .name = ".."};
        put_block(dev, bno, buf);
    }

    enter_name(pip, ino, base);

    iput(mip);
    iput(pip);
}

void makedir()
{
    make_file(1);
}

void create_file()
{
    make_file(0);
}

void quit()
{
    // iput() all minodes with (refCount > 0 && DIRTY)
    for (int i = 0; i < NMINODE; i++)
        if (minode[i].refCount > 0 && minode[i].dirty == 1)
        {
            minode[i].refCount = 1;
            iput(minode + i);
        }
    exit(0); 
}

int rmchild(MINODE * pip)
{
    // get help pls
}

int rmdir()
{
    dbname(pathname);
    int ino = getino(pathname);
    MINODE * mip = iget(dev, ino);
    if(proc->uid != mip->inode.i_uid && /*NOT SUPER USER*/)
    {
        printf("You do not have permission to remove %s.\n", pathname);
        return 1;
    }
    if(!S_ISDIR(mip->inode.i_mode))
    {
        printf("%s is not a directory", pathname);
    }
    if(mip->inode.i_links_count > 2)
    {
        printf("Directory %s is not empty.\n", pathname);
        return 1;
    }
    for(int i = 2; i < 12; i++)
    {
        if(mip->inode.i_block[i] != 0)
        {
            printf("Directory %s is not empty.\n", pathname);
            return 1;
        }
    }
    //TODO: Check for busy

    for(int i = 0; i < 12)
    {
        if (mip->inode.i_block[i] == 0)
            continue;
        bdalloc(mip->dev,mip->inode.i_block[i])
    }
    idealloc(mip->dev, mip->ino);
    iput(mip);
    
    MINODE * pip = iget(dname);
    rm_child(pip)
}


int main(void)
{
    init();
    mount_root();


    while(1)
    {
        printf("\033[1;34mdash\033[0m $");
        fgets(line,256,stdin);
        line[strlen(line)-1] = '\0';
        sscanf(line, "%s %s", cmd, pathname);
    }
}