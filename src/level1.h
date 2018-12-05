#pragma once
#include "type.h"

void mychdir();
void ls_file(int inom, char * filename);
void ls_dir(char * dirname);
void ls();
void rpwd(MINODE *wd);
void pwd();
int ideal_length(int name_len);
void enter_name(MINODE * pip, int myino, char * myname);
int make_entry(int dir, char * name);
void makedir();
void create_file();
int rmchild(MINODE * pip, char * name);
void rmdir();
void link();
void unlink();
void symlink();
void readlink();
void mystat();
void mychmod();