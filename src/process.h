/*
* This file defines the struct that represents
* processes in the filesystem project
*/
#pragma once

#include "filesystem.h"

typedef struct proc
{
    struct proc * next;
    int pid, uid, gid;
    Minode cwd;
}Proc;
