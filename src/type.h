#pragma once
#include <ext2fs/ext2_fs.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc  GD;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;   

#define BLKSIZE  1024
#define NMINODE    64
#define NOFT       64
#define NFD        10
#define NMOUNT      4
#define NPROC       2

typedef struct minode{
  INODE inode;
  int dev, ino, refCount, dirty, mounted;
  struct mntable *mptr;
}MINODE;

typedef struct oft{
  int  mode, refCount, offset;
  MINODE *mptr;
}OFT;

typedef struct proc{
  struct proc *next;
  int          pid, uid, gid;
  MINODE      *cwd;
  OFT         *fd[NFD];
}PROC;