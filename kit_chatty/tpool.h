#ifndef TPOOL_H
#define TPOOL_H

#include <pthread.h>
#include <ops.h>
#include <message.h>
#include <hashmap.h>

extern char * UnixPath;
extern char * DirName;
extern char * StatFileName;
extern int MaxConnections;
extern int ThreadsInPool;
extern int MaxMsgSize;
extern int MaxFileSize;
extern int MaxHistMsgs;
extern pthread_mutex_t *fd_lock;
extern pthread_mutex_t wMsgs_lock;

typedef struct tpool_work {
	message_hdr_t hdr;
	long connfd;
	struct tpool_work *next;
} tpool_work_t;

typedef struct tpool {
	int num_threads;
	int max_q_size;

	pthread_t *threads;
	int cur_q_size;
	tpool_work_t *q_head;
	tpool_work_t *q_tail;
	pthread_mutex_t q_lock;
	pthread_cond_t q_not_empty;
	pthread_cond_t q_not_full;
	pthread_cond_t q_empty;
	int q_closed;
	int shutdown;
	hashmap_t tab;
	int max_conn;
} *tpool_t;

int tpool_init(tpool_t *tpool,int num_worker_threads, int max_q_size, hashmap_t tab,int max_conn);

int tpool_add_work(tpool_t tpool, long connfd,message_hdr_t hdr);

int tpool_destroy(tpool_t *tpool, int finish);

int connected_user(tpool_t tpool,char **buf);

int tpool_search_fd(tpool_t tpool,long fd);

int sendTxt(node_t rcv,message_t *reply,tpool_work_t *my_workp,tpool_t tpool);	

#endif	// TPOOL_H

