#ifndef HASHMAP_H
#define HASHMAP_H

#include <config.h>
#include <message.h>
#include <pthread.h>
#include <ops.h>



typedef struct node {
	char user[MAX_NAME_LENGTH];
	struct node *next;
	int fdonline;
	message_t *msgs;
	int msgsToRead;
} *node_t;

typedef struct hashmap {
	int size;
	node_t *list;
	int MaxHistMsgs;
} *hashmap_t;

int hash_init(hashmap_t *tab, int size,int MaxHistMsgs);

int hash_foo(const char *nick,hashmap_t tab);

int hash_ins(hashmap_t *tab, const char *nick,int fdonline);

node_t hash_search(hashmap_t tab, const char *nick,int fd);

int hash_destroy(hashmap_t tab, const char *nick);

int hash_free(hashmap_t *tab);

int hash_status(hashmap_t tab, const char *nick,int s);

int hash_print(hashmap_t tab,int maxconn,int *nusers,char **lista);

#endif	// HASHMAP_H

