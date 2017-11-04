#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include "parsing.h"

int parsing(FILE *fp){
	
	char bufr[MAXLINE];
    char auxInt[MAXLINE];
    char path[MAXLINE];
	
	while(! feof(fp)){
			fgets(bufr, MAXLINE, fp);
			if(strncmp(bufr,"UnixPath",8) == 0){
				int i=0;
				while(bufr[i] != '/')				
					i++;	
		
				int j=0;
				while(bufr[i] != '\n'){
					path[j] = bufr[i];
					j++;
					i++;
				}
				path[j]='\0';
				UnixPath=(char *) malloc(sizeof(char)*(strlen(path)+1));
				strcpy(UnixPath,path);

			}

			if(strncmp(bufr,"MaxConnections",14) == 0){
				int i=0;
				while(bufr[i] != '=')				
					i++;	
				i+=2;
				int j=0;
				while(bufr[i] != '\n'){
					auxInt[j] = bufr[i];
					j++;
					i++;
				}
				auxInt[j]='\0';
				MaxConnections = strtol(auxInt,NULL,10);

			}
			
			if(strncmp(bufr,"ThreadsInPool",13) == 0){
				int i=0;
				while(bufr[i] != '=')				
					i++;	
				i+=2;
				int j=0;
				while(bufr[i] != '\n'){
					auxInt[j] = bufr[i];
					j++;
					i++;
				}
				auxInt[j]='\0';
				ThreadsInPool = strtol(auxInt,NULL,10);

			}
			
			if(strncmp(bufr,"MaxMsgSize",10) == 0){
				int i=0;
				while(bufr[i] != '=')				
					i++;	
				i+=2;
				int j=0;
				while(bufr[i] != '\n'){
					auxInt[j] = bufr[i];
					j++;
					i++;
				}
				auxInt[j]='\0';
				MaxMsgSize = strtol(auxInt,NULL,10);

			}
			
			if(strncmp(bufr,"MaxFileSize",11) == 0){
				int i=0;
				while(bufr[i] != '=')				
					i++;	
				i+=2;
				int j=0;
				while(bufr[i] != '\n'){
					auxInt[j] = bufr[i];
					j++;
					i++;
				}
				auxInt[j]='\0';
				MaxFileSize = strtol(auxInt,NULL,10);

			}
			
			if(strncmp(bufr,"MaxHistMsgs",11) == 0){
				int i=0;
				while(bufr[i] != '=')				
					i++;	
				i+=2;
				int j=0;
				while(bufr[i] != '\n'){
					auxInt[j] = bufr[i];
					j++;
					i++;
				}
				auxInt[j]='\0';
				MaxHistMsgs = strtol(auxInt,NULL,10);

			}
			
			if(strncmp(bufr,"DirName",7) == 0){
				memset(path,0,MAXLINE);
				int i=0;
				while(bufr[i] != '=')				
					i++;	
				i+=2;
				int j=0;
				while(bufr[i] != '\n'){
					path[j] = bufr[i];
					j++;
					i++;
				}
				path[j]='\0';
				DirName = (char *) malloc(sizeof(char)*(strlen(path)+1));
				strcpy(DirName,path);

			}
			
			if(strncmp(bufr,"StatFileName",12) == 0){
				memset(path,0,MAXLINE);
				int i=0;
				while(bufr[i] != '=')				
					i++;	
				i+=2;
				int j=0;
				while(bufr[i] != '\n'){
					path[j] = bufr[i];
					j++;
					i++;
				}
				auxInt[j]='\0';
				StatFileName = (char *) malloc(sizeof(char)*(strlen(path)+1));
				strcpy(StatFileName,path);

			}
 	   }
	
	
	return 0;
}
