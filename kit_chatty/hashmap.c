#include <ctype.h>
#include <hashmap.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


extern char * UnixPath;
extern char * DirName;
extern char * StatFileName;
extern int MaxConnections;
extern int ThreadsInPool;
extern int MaxMsgSize;
extern int MaxFileSize;
extern int MaxHistMsgs;
extern pthread_mutex_t *fd_lock;
extern int epollfd,nfds;
extern struct epoll_event ev;

/*
int hash_index_mutex(const char *nick, hashmap_t tab){
	int index = hash_foo
}*/

int hash_foo(const char *nick, hashmap_t tab)
{
    unsigned long c,i,h;

    for(i=h=0;nick[i];i++)
    {
        c = toupper(nick[i]);
        h = (((h << 5) + h) ^ c)% tab->size;
    }
    return h;
}

int hash_init(hashmap_t *tab, int size,int MaxHistMsgs){

	if(((*tab) = (hashmap_t) malloc(sizeof(struct hashmap))) == NULL){
		perror("malloc");
		return -1;
	}

	if(((*tab)->list = (node_t*) malloc(sizeof(node_t)*size)) == NULL){
		perror("malloc");
		return -1;
	}

	for(int i=0;i<size;i++){
		(*tab)->list[i] = NULL;
	}

	(*tab)->size=size;
	(*tab)->MaxHistMsgs=MaxHistMsgs;

	return 0;
}


int hash_ins(hashmap_t *tab, const char *nick,int fdonline){
	int i = hash_foo(nick,*tab);
	int nick_len = strlen(nick);

	if((*tab)->list[i] == NULL){
		(*tab)->list[i] = malloc(sizeof(struct node));
		strncpy((*tab)->list[i]->user,nick,nick_len);
		(*tab)->list[i]->user[nick_len]='\0';
		(*tab)->list[i]->next=NULL;
		pthread_mutex_lock(&fd_lock[fdonline - epollfd]);
		(*tab)->list[i]->fdonline=fdonline;
		pthread_mutex_unlock(&fd_lock[fdonline- epollfd]);
		(*tab)->list[i]->msgsToRead=0;
		(*tab)->list[i]->msgs = calloc(((*tab)->MaxHistMsgs),sizeof(message_t));
	/*	for(int j=0;i<(*tab)->MaxHistMsgs;i++){
			(*tab)->list[i]->msgs[j].data.buf=NULL;			
		}*/
	
	}else{
		node_t curr=(*tab)->list[i];
		while(curr->next != NULL){
			curr=curr->next;					
		}
		curr->next=malloc(sizeof(struct node));
		strncpy(curr->next->user,nick,nick_len);
		curr->next->user[nick_len]='\0';
		curr->next->next=NULL;
		pthread_mutex_lock(&fd_lock[fdonline - epollfd]);
		curr->next->fdonline=fdonline;
		pthread_mutex_unlock(&fd_lock[fdonline - epollfd]);
		curr->next->msgsToRead=0;
		curr->next->msgs = calloc((*tab)->MaxHistMsgs,sizeof(message_t));
	}
	return 0;
}


node_t hash_search(hashmap_t tab, const char *nick,int fd){
	if(fd<=0){
		int i = hash_foo(nick,tab);
		int nick_len = strlen(nick);	
		if(tab->list[i] != NULL){
			if(strncmp(tab->list[i]->user,nick,nick_len) == 0)	
				return tab->list[i];
			else{
				node_t curr=tab->list[i];
				while(curr->next != NULL && (strncmp(curr->user,nick,nick_len) != 0)){
					curr=curr->next;					
				}
				if(strncmp(curr->user,nick,nick_len) == 0)
					return curr;
			}
		}
	}else if (fd > 0){
		for(int i=0;i<tab->size;i++){
			if(tab->list[i] != NULL){
				if(tab->list[i]->fdonline == fd)	
					return tab->list[i];
				else{
					node_t curr=tab->list[i];
					while(curr->next != NULL && (curr->fdonline != fd)){
						curr=curr->next;					
					}
					if(curr->fdonline == fd)
						return curr;
				}
			}
		}
	}
	return NULL;
}

int hash_destroy(hashmap_t tab, const char *nick){
	int i = hash_foo(nick,tab);
	int nick_len = strlen(nick);
	
	if(tab->list[i] != NULL){
		if(strncmp(tab->list[i]->user,nick,nick_len) == 0)	//testa
			if(tab->list[i]->next == NULL){ //senza collisioni
				free(tab->list[i]);
				tab->list[i] = NULL;
			}else{
				node_t curr=tab->list[i];
				tab->list[i]=tab->list[i]->next;			
				free(curr);
			}
		else{
			node_t curr=tab->list[i];
			node_t prec=NULL;
			while(curr->next != NULL && (strncmp(curr->user,nick,nick_len) != 0)){
				prec=curr;				
				curr=curr->next;					
			}
			if(strncmp(curr->user,nick,nick_len) == 0){
				prec->next=curr->next;
				free(curr);
			}
				
		}
	}
	return 0;
}

int hash_free(hashmap_t *tab){
	for(int i=0;i<(*tab)->size;i++){	
		if((*tab)->list[i] != NULL){
			if((*tab)->list[i]->next == NULL){ //senza collisioni
				int j=(*tab)->list[i]->msgsToRead;
				while(j>0){
						free((*tab)->list[i]->msgs[j-1].data.buf);
						j--;
				}
				free((*tab)->list[i]->msgs);
				free((*tab)->list[i]);
				(*tab)->list[i] = NULL;
			}else{
				node_t curr=(*tab)->list[i];
				while(curr != NULL){
					(*tab)->list[i]=(*tab)->list[i]->next;
					int j=curr->msgsToRead;
					while(j>0){
						free(curr->msgs[j-1].data.buf);
						j--;
					}
					free(curr->msgs);		
					free(curr);
					curr=(*tab)->list[i];
				}
			}
		}
	}	
	free((*tab)->list);
	free(*tab);
	return 0;
}

int hash_status(hashmap_t tab, const char *nick,int s){//cambia node.fdonline, s=0 -> offline
    //int i = hash_foo(nick,tab);
	//int nick_len = strlen(nick);
	int fd;
	node_t rcv=hash_search(tab,nick,0);
	
	if(rcv != NULL){
		fd=rcv->fdonline?rcv->fdonline:s;
	
		if(!(fd == 0 && s == 0)){
			pthread_mutex_lock(&fd_lock[fd - epollfd]); //i fd degli utenti partono da epollfd + 1
			rcv->fdonline=s;
			pthread_mutex_unlock(&fd_lock[fd - epollfd]);
			return 0;
		}

		printf("Utente %s già offline!\n",nick);
		fflush(stdout);
		return 1;
	}
	printf("Utente %s inesistente!\n",nick);
	fflush(stdout);
	
	return 1;

}

int hash_print(hashmap_t tab,int maxconn,int *nusers,char **str){
	
	char *lista = malloc(sizeof(char)* maxconn * (MAX_NAME_LENGTH+1));
	memset(lista,0,sizeof(char)* maxconn * (MAX_NAME_LENGTH+1));	
	*nusers=0;

	for(int i = 0 ; i<tab->size; i++){
		if(tab->list[i] != NULL){
			if(tab->list[i]->fdonline >0){//se si disconnette ora non fa nulla
				strncat(lista+(*nusers * (MAX_NAME_LENGTH+1)),tab->list[i]->user,strlen(tab->list[i]->user)); //per non sovrascrivere '/0'
				*nusers+=1;
			}
			
			node_t curr=tab->list[i];
			while(curr->next != NULL ){
				if(curr->next->fdonline >0){
					strncat(lista+(*nusers * (MAX_NAME_LENGTH+1)),curr->next->user,strlen(curr->next->user));
					*nusers+=1;
				}
				curr=curr->next;					
			}
			
		}
	}
	*str=lista;	
	return 0;
}
