#pragma once
#include "type.h"

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char gpath[256];
extern char *name[64];
extern int n;
extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256], pathname2[256], dname[256], bname[256];
extern int data_start;
extern OFT oft[NOFT];