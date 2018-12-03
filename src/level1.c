#include "level1.h"

bool mount_root(const char * name, Filesystem * fs, Proc * super, Proc * reg, Proc ** running)
{
    *fs = Filesystem_Init(name);
    if(fs->OK == false)
        return false;
    super->cwd = reg->cwd = fs->root;
    *running = super;
    return true;
}

Error mkdir(const char * path, Filesystem * fs, const Proc * caller)
{
    
}