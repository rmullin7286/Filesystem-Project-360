/*
* This file contains operations that satisfy level 1
* of the 360 filesystem project
*/

#include "process.h"
#include "filesystem.h"
#include "error.h"

/*
* Opens fs using parameter name
* sets super and reg proc cwd to the root Minode of fs
* sets running to point at super
* returns false if fs could not be loaded. Otherwise true
*/
bool mount_root(const char * name, Filesystem * fs, Proc * super, Proc * reg, Proc ** running);

/*
* Makes a directory at the given path
* Returns OK if the directory was created
* Returns ERROR_INVALID_PERMISSIONS if the caller does not have the correct permissions
* Returns ERROR_PATH_EXISTS if there is already a file at the given path
*/
Error mkdir(const char * path, Filesystem * fs, const Proc * caller);