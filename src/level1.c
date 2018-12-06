#include "level1.h"
#include "globals.h"
#include "util.h"

#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>

void mychdir()
{
    if (strlen(pathname) == 0 || strcmp(pathname, "/") == 0)
    {
        iput(running->cwd);
        running->cwd = root;
        return;
    }
    else
    {
        char temp[256];
        strcpy(temp,pathname);
        if(pathname[0] == '/')
        {
            dev = root->dev;
        }
        else
            dev = running->cwd->dev;
        int ino = getino(temp);
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

void ls_file(int ino, char *filename)
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

    time_t t = (time_t)(mip->inode.i_ctime);
    char temp[256];
    strcpy(temp, ctime(&t))[strlen(temp) - 1] = '\0';

    printf(" %4d %4d %4d %4d %s %s", (int)(mip->inode.i_links_count), (int)(mip->inode.i_gid), (int)(mip->inode.i_uid), (int)(mip->inode.i_size),
            temp, filename);

    if(S_ISLNK(mip->inode.i_mode))
        printf(" -> %s", (char*)(mip->inode.i_block));

    putchar('\n');

    iput(mip);   
}

void ls_dir(char * dirname)
{
    int ino = getino(dirname);
    MINODE * mip = iget(dev, ino);
    char buf[BLKSIZE];
    for(int i = 0; i < 12 && mip->inode.i_block[i]; i++)
    {
        get_block(dev, mip->inode.i_block[i], buf);
        char * cp = buf;
        DIR * dp = (DIR*)buf;

        while(cp < BLKSIZE + buf)
        {
            char temp[256];
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            ls_file(dp->inode, temp);
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }
    }
    iput(mip);
}

void ls()
{
    char temp[256];
    int ino;
    if(strlen(pathname) == 0)
    {
        strcpy(pathname, ".");
        ino = running->cwd->ino;
    }
    else
    {
        strcpy(temp, pathname);
        ino = getino(temp);
    }
    MINODE *mip = iget(dev, ino);

    if(S_ISDIR(mip->inode.i_mode))
    {
        ls_dir(pathname);
    }
    else
    {
        ls_file(ino, pathname);
    }
    iput(mip);
}

void rpwd(MINODE *wd)
{
    char buf[256];
    if (wd==root)
        return;
    int parent_ino = search(wd,"..");
    int my_ino = wd->ino;
    MINODE * pip = iget(dev, parent_ino);
    findmyname(pip, my_ino, buf);
    rpwd(pip);
    iput(pip);
    printf("/%s", buf);
}

void pwd()
{
    if(running->cwd == root)
    {
        puts("/");
        return;
    }
    rpwd(running->cwd);
    putchar('\n');
    // if (wd == root) print "/"
    // else
    // rpwd(wd);
}

int ideal_length(int name_len)
{
    return 4 * ((8 + name_len + 3) / 4);
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
            put_block(pip->dev, pip->inode.i_block[i], buf);
            return;
        }
 
        get_block(pip->dev, pip->inode.i_block[i], buf);

        //get to the last entry in the block
        while(cp + dp->rec_len < buf + BLKSIZE)
        {
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }

        int remain = dp->rec_len - ideal_length(dp->name_len);
        if(remain >= need_len)
        {
            //put entry in existing block
            dp->rec_len = ideal_length(dp->name_len);
            cp += dp->rec_len;
            dp = (DIR*)cp;
            *dp = (DIR){.inode = myino, .rec_len = remain, .name_len = strlen(myname)};
            strncpy(dp->name, myname, dp->name_len);
            put_block(pip->dev, pip->inode.i_block[i], buf);
            pip->dirty = 1;
            return;
        }
    }
}

int make_entry(int dir, char * name)
{
    dbname(name);
    int parent = getino(dname);
    if(!parent)
    {
        printf("ERROR: Specified path does not exist\n");
        return 0;
    }
    MINODE * pip = iget(dev, parent);
    if(!S_ISDIR(pip->inode.i_mode))
    {
        printf("ERROR: Specified path is not a directory\n");
        return 0;
    }
    if(search(pip, bname))
    {
        printf("ERROR: Entry %s already exists\n", bname);
        return 0;
    }

    if(dir)
        pip->inode.i_links_count++;
    
    int ino = ialloc(dev), bno = dir ? balloc(dev) : 0;

    MINODE * mip = iget(dev, ino);
    INODE * ip = &(mip->inode);
    time_t t = time(0L);
    *ip = (INODE){.i_mode = (dir ? 0x41ED : 0x81A4), .i_uid = running->uid, .i_gid = running->gid, .i_size = (dir ? BLKSIZE : 0), .i_links_count = (dir ? 2 : 1),
            .i_atime = t, .i_ctime = t, .i_mtime = t, .i_blocks = (dir ? 2 : 0), .i_block = {bno}};
    mip->dirty = 1;

    if(dir)
    {
        char buf[BLKSIZE] = {0};
        char * cp = buf;
        DIR * dp = (DIR*)buf;
        *dp = (DIR){.inode = ino, .rec_len = 12, .name_len = 1, .name = "."};
        cp += dp->rec_len;
        dp = (DIR*)cp;
        *dp = (DIR){.inode = pip->ino, .rec_len = BLKSIZE - 12, .name_len = 2, .name = ".."};
        put_block(dev, bno, buf);
    }

    enter_name(pip, ino, bname);

    iput(mip);
    iput(pip);

    return ino;
}

void makedir()
{
    make_entry(1, pathname);
}

void create_file()
{
    make_entry(0, pathname);
}

int rmchild(MINODE * pip, char * name)
{
    char *cp, buf[BLKSIZE];
    DIR *dp, *prev;
    int current;

    for(int i = 0; i < 12 && pip->inode.i_block[i]; i++)
    {
        current = 0;

        get_block(dev, pip->inode.i_block[i], buf);
        cp = buf;
        dp = (DIR *) buf;
        prev = 0;
        while (cp < buf + BLKSIZE)
        {
            if (strncmp(dp->name, name, dp->name_len) == 0)
            {
                int ideal = ideal_length(dp->name_len);
                if(ideal != dp->rec_len) // Last entry since rec_len is just taking the rest of the block
                {
                    // Expand previous entry to take up deleted space.
                    prev->rec_len += dp->rec_len;
                }
                else if(prev == NULL && cp + dp->rec_len == buf + 1024)// First and only entry because prev has not moved and dp takes the entire block
                {
                    char empty[BLKSIZE] = {0};
                    put_block(dev, pip->inode.i_block[i], empty);
                    bdalloc(dev, pip ->inode.i_block[i]); // Boof the entire block
                    pip->inode.i_size -= BLKSIZE; // Decrement the block by the entire size of a block
                    for(int j = i; j < 11; j++)
                    {
                        pip->inode.i_block[j] = pip->inode.i_block[j+1];
                    }
                    pip->inode.i_block[11] = 0;
                }
                else
                {
                    // Case where it is not (first and only) or last
                    int removed = dp->rec_len;
                    char *temp = buf;
                    DIR *last = (DIR *)temp;

                    while(temp + last->rec_len < buf + BLKSIZE) // Move to last entry in dir
                    {
                        temp += last->rec_len;
                        last = (DIR *) temp;
                    }
                    last->rec_len = last->rec_len + removed;

                    // Move everything after the removed entry to the left by the length of the entry
                    memcpy(cp, cp + removed, BLKSIZE - current - removed + 1);
                }
                put_block(dev, pip->inode.i_block[i], buf);
                pip->dirty = 1;
                return 1;
            }
            cp += dp->rec_len; // move to next
            current += dp->rec_len;
            prev = dp;
            dp = (DIR *) cp;
        }
        return 0;
    }

    printf("ERROR: %s does not exist.\n",bname);
    return 1;
}

void rmdir()
{
    dbname(pathname);
    char *temp = strdup(pathname);
    int ino = getino(temp); //2. Get ino of pathname
    free(temp);
    if(!ino)
    {
        printf("ERROR: %s does not exist.\n", pathname);
        return;
    }
    MINODE * mip = iget(dev, ino); //3. Get ino
    if(running->uid != mip->inode.i_uid && running->uid != 0)
    {
        printf("ERROR: You do not have permission to remove %s.\n", pathname);
        iput(mip);
        return;
    }
    if(!S_ISDIR(mip->inode.i_mode))
    {
        printf("ERROR: %s is not a directory", pathname);
        iput(mip);
        return;
    }
    if(mip->inode.i_links_count > 2)
    {
        printf("ERROR: Directory %s is not empty.\n", pathname);
        iput(mip);
        return;
    }
    if(mip->inode.i_links_count == 2)
    {
        char buf[BLKSIZE], * cp;
        DIR *dp;
        get_block(dev, mip->inode.i_block[0],buf);
        cp = buf;
        dp = (DIR *) buf;
        cp += dp->rec_len;
        dp = (DIR *) cp; // Move to second entry ".."
        if(dp->rec_len != 1012)
        {
            printf("ERROR: Directory %s is not empty.\n", pathname);
            iput(mip);
            return;
        }
    }

    for(int i = 0; i < 12; i++)
    {
        if (mip->inode.i_block[i] == 0)
            continue;
        bdalloc(mip->dev,mip->inode.i_block[i]);
    }
    idalloc(mip->dev, mip->ino);
    iput(mip);
    
    MINODE * pip = iget(dev, getino(dname));
    rmchild(pip, bname);

    pip->inode.i_links_count--;
    pip->inode.i_atime = pip->inode.i_mtime = time(0L);
    pip->dirty = 1;
    iput(pip);
}

void link()
{
    int ino = getino(pathname);
    if(!ino)
    {
        printf("Source file does not exist\n");
        return;
    }
    MINODE * source = iget(dev, ino);
    if(S_ISDIR(source->inode.i_mode))
    {
        printf("Link to directory not allowed\n");
        return;
    }
    dbname(pathname2);
    int dirino = getino(dname);
    if(!dirino)
    {
        printf("Destination directory does not exist\n");
        return;
    }
    MINODE * dirnode = iget(dev, dirino);
    if(!S_ISDIR(dirnode->inode.i_mode))
    {
        printf("Destination is not a directory\n");
        return;
    }
    char *base = strdup(bname);
    if(search(dirnode, base))
    {
        printf("Destination file already exists\n");
        return;
    }
    free(base);
    enter_name(dirnode, ino, bname);

    source->inode.i_links_count++;
    source->dirty = 1;

    iput(source);
    iput(dirnode);
}

void unlink()
{
    int ino = getino(pathname);
    if(!ino)
    {
        printf("Could not find specified path\n");
        return;
    }
    MINODE * m = iget(dev, ino);
    if(S_ISDIR(m->inode.i_mode))
    {
        printf("Specified path is a directory\n");
        return;
    }
    if(--(m->inode.i_links_count) == 0)
    {
        mytruncate(m);
        idalloc(m->dev, m->ino);
    }
    else
    {
        m->dirty = 1;
    }

    dbname(pathname);
    MINODE * parent = iget(dev, getino(dname));
    rmchild(parent, bname);

    iput(parent);
    iput(m);
}

void symlink()
{
    char * pathname_dup = strdup(pathname);
    if(!getino(pathname_dup))
    {
        free(pathname_dup);
        printf("Specified source path does not exist\n");
        return;
    }
    free(pathname_dup);
    int ino = make_entry(0, pathname2);
    MINODE * m = iget(dev, ino);
    m->inode.i_mode = 0xA1A4;
    strcpy((char*)(m->inode.i_block), pathname);

    iput(m);
}

void readlink()
{
    int ino = getino(pathname);
    if(!ino)
    {
        printf("Specified path does not exist\n");
        return;
    }
    MINODE * m = iget(dev, ino);
    if(!S_ISLNK(m->inode.i_mode))
    {
        printf("Specified path is not a link file\n");
        return;
    }
    printf("%s\n", (char*)(m->inode.i_block));

    iput(m);
}

void mystat()
{
    int ino = getino(pathname);
    if(!ino)
    {
        printf("File not found\n");
        return;
    }
    MINODE * m = iget(dev, ino);
    dbname(pathname);
    printf("File: %s\n", bname);
    printf("Size: %d\n", m->inode.i_size);
    printf("Blocks: %d\n", m->inode.i_blocks);
    char * type;
    if(S_ISDIR(m->inode.i_mode))
        type = "Dir";
    else if(S_ISLNK(m->inode.i_mode))
        type = "Link";
    else
        type = "Reg";

    time_t a = (time_t) m->inode.i_atime, c = (time_t) m->inode.i_ctime, mod = (time_t) m->inode.i_atime;
    printf("Type: %s\n", type);
    printf("Inode: %d\n", ino);
    printf("Links: %d\n", m->inode.i_links_count);
    printf("Access Time: %s\n", ctime(&a));
    printf("Modify Time: %s\n", ctime(&mod));
    printf("Change Time: %s\n", ctime(&c));
    printf("Device: %d\n", m->dev);
    printf("UID: %d\n", m->inode.i_uid);
    printf("GID: %d\n", m->inode.i_gid);

    //TODO: ACCESS TIME

    iput(m);
}

void mychmod()
{
    int ino = getino(pathname);
    if(!ino)
    {
        printf("File does not exist\n");
        return;
    }
    MINODE * m = iget(dev, ino);
    int new;
    sscanf(pathname2, "%o", &new);
    
    int old = m->inode.i_mode;
    old >>= 9;
    new |= (old << 9);

    m->inode.i_mode = new;
    m->dirty = 1;

    iput(m);
}