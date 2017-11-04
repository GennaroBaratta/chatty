#ifndef PARSING_H_
#define PARSING_H_

#include <stdio.h>

extern char * UnixPath;
extern char * DirName;
extern char * StatFileName;
extern int MaxConnections;
extern int ThreadsInPool;
extern int MaxMsgSize;
extern int MaxFileSize;
extern int MaxHistMsgs;

int parsing(FILE * fp);

#endif

