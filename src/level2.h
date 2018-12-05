#pragma once

void myopen();
int open_file(char * name, int mode);
void myclose();
void close_file(int fd);
int myread(int fd, char * buf, int nbytes);
void read_file();
void mytouch();
void touch_file(char * name);
void mylseek(int fd, int position);
void write_file();
int mywrite(int fd, char buf[], int nbytes);
void cp();
void mycat();
void mymov();