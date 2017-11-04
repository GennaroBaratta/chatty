#include <tpool.h>
#include <stdio.h>
#include <stdlib.h>
#include <message.h>
#include <connections.h>
#include <ops.h>
#include <unistd.h>
#include <config.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>


#define SYSCALL(r,c,e) \
    if((r=c)==-1) { perror(e);exit(errno); }

extern int MaxConnections;
extern int epollfd, nfds;
extern struct epoll_event ev;


static void * tpool_thread(tpool_t tpool);

int tpool_init(tpool_t *tpoolp, int num_worker_threads, int max_q_size, hashmap_t tab, int max_conn) {
    int i, notused;
    tpool_t tpool;

    if ((tpool = (tpool_t) malloc(sizeof (struct tpool))) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    tpool->num_threads = num_worker_threads;
    tpool->max_q_size = max_q_size;

    if ((tpool->threads = (pthread_t*) malloc(sizeof (pthread_t) * num_worker_threads)) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }



    tpool->q_head = NULL;
    tpool->q_tail = NULL;
    tpool->q_closed = 0;
    tpool->cur_q_size = 0;
    tpool->shutdown = 0;
    tpool->tab = tab;
    tpool->max_conn = max_conn;

    if (pthread_mutex_init(&(tpool->q_lock), NULL)) {
        fprintf(stderr, "Could not initialize my_lock");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&(tpool->q_not_empty), NULL)) {
        fprintf(stderr, "Could not initialize my_lock");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&(tpool->q_not_full), NULL)) {
        fprintf(stderr, "Could not initialize my_lock");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&(tpool->q_empty), NULL)) {
        fprintf(stderr, "Could not initialize my_lock");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i != num_worker_threads; i++) {
        if ((notused = pthread_create(&(tpool->threads[i]), NULL, (void * (*)(void *))tpool_thread, (void*) tpool)) != 0) {
            fprintf(stderr, "pthread_create %d\n", notused);
            exit(EXIT_FAILURE);
        }
    }

    *tpoolp = tpool;

    return 0;

}

static void * tpool_thread(tpool_t tpool) {
    tpool_work_t *my_workp;


    while (1) {

        pthread_mutex_lock(&(tpool->q_lock));

        while ((tpool->cur_q_size == 0) && (!tpool->shutdown)) {
            pthread_cond_wait(&(tpool->q_not_empty), &(tpool->q_lock));
        }

        if (tpool->shutdown) {
            pthread_mutex_unlock(&(tpool->q_lock));
            return NULL;
        }

        my_workp = tpool->q_head;

        tpool->cur_q_size--;
        if (tpool->cur_q_size == 0)
            tpool->q_head = tpool->q_tail = NULL;
        else
            tpool->q_head = my_workp->next;

        if ((tpool->cur_q_size == (tpool->max_q_size - 1)))
            pthread_cond_broadcast(&(tpool->q_not_full));

        if (tpool->cur_q_size == 0)
            pthread_cond_signal(&(tpool->q_empty));
        pthread_mutex_unlock(&(tpool->q_lock));

        message_t reply;
        reply.data.buf = NULL;

        int nusers;

        message_data_t data;
        data.buf = NULL;
        /*	data.hdr.len=0;
                memset(&(data.hdr.receiver),0,MAX_NAME_LENGTH+1);*/
        int notused = 0;

       // if ((strncmp(my_workp->hdr.sender, "SERVER", strlen("SERVER"))) != 0) {
            if ((notused = readData(my_workp->connfd, &data)) < 0) {
                fprintf(stderr, "ERRORE in thread: lettura messaggio\n");
                exit(EXIT_FAILURE);
            }
       /* } else {
            node_t rcv = hash_search(tpool->tab, "", my_workp->connfd);
            if (rcv != NULL) {
                strncpy(my_workp->hdr.sender, rcv->user, strlen(rcv->user));
            } else {

            }
        }*/
        char *ListaOn = NULL;

        switch (my_workp->hdr.op) {
            case USRLIST_OP:
            {
                setHeader(&reply.hdr, OP_OK, "");
                hash_print(tpool->tab, tpool->max_conn, &nusers, &ListaOn);
                setData(&reply.data, "", ListaOn, nusers * (MAX_NAME_LENGTH + 1));
                sendHeader(my_workp->connfd, &(reply.hdr));
                sendData(my_workp->connfd, &(reply.data));
                free(ListaOn);
                break;
            }

            case REGISTER_OP:
            {
                if (hash_search(tpool->tab, my_workp->hdr.sender, 0) == NULL) {
                    hash_ins(&(tpool->tab), my_workp->hdr.sender, my_workp->connfd);
                    setHeader(&reply.hdr, OP_OK, "");
                    hash_print(tpool->tab, tpool->max_conn, &nusers, &ListaOn);
                    setData(&reply.data, "", ListaOn, nusers * (MAX_NAME_LENGTH + 1));
                    sendHeader(my_workp->connfd, &(reply.hdr));
                    sendData(my_workp->connfd, &(reply.data));
                    free(ListaOn);
                } else {
                    setHeader(&reply.hdr, OP_NICK_ALREADY, "");
                    setData(&reply.data, "", NULL, 0);
                    sendHeader(my_workp->connfd, &(reply.hdr));
                }

                break;
            }
            case CONNECT_OP:
            {
                if (hash_search(tpool->tab, my_workp->hdr.sender, 0) != NULL) {
                    hash_status(tpool->tab, my_workp->hdr.sender, my_workp->connfd);
                    setHeader(&reply.hdr, OP_OK, "");
                    hash_print(tpool->tab, tpool->max_conn, &nusers, &ListaOn);
                    setData(&reply.data, "", ListaOn, nusers * (MAX_NAME_LENGTH + 1));
                    sendHeader(my_workp->connfd, &(reply.hdr));
                    sendData(my_workp->connfd, &(reply.data));
                    free(ListaOn);
                } else {
                    setHeader(&reply.hdr, OP_NICK_UNKNOWN, "");
                    setData(&reply.data, "", NULL, 0);
                    sendHeader(my_workp->connfd, &(reply.hdr));
                }

                break;
            }
            case POSTTXT_OP:
            {
                if (data.hdr.len > MaxMsgSize) {
                    setHeader(&reply.hdr, OP_MSG_TOOLONG, "");
                    setData(&reply.data, "", NULL, 0);
                    sendHeader(my_workp->connfd, &(reply.hdr));
                    break;
                }
                //printf("Preparazione invio testo...\n");
                //makwfflush(stdout);
                node_t rcv = hash_search(tpool->tab, data.hdr.receiver, 0);
                if (hash_search(tpool->tab, my_workp->hdr.sender, 0) != NULL && rcv != NULL) {
                    setHeader(&reply.hdr, TXT_MESSAGE, my_workp->hdr.sender);
                    setData(&reply.data, data.hdr.receiver, data.buf, data.hdr.len);

                    if (sendTxt(rcv, &reply, my_workp, tpool) == 0) {
                        setHeader(&reply.hdr, OP_OK, "");
                        sendHeader(my_workp->connfd, &(reply.hdr));
                    } else {
                        setHeader(&reply.hdr, OP_FAIL, "");
                        setData(&reply.data, "", NULL, 0);
                        sendHeader(my_workp->connfd, &(reply.hdr));
                    }
                } else {
                    setHeader(&reply.hdr, OP_FAIL, "");
                    setData(&reply.data, "", NULL, 0);
                    sendHeader(my_workp->connfd, &(reply.hdr));
                }
                break;
            }
            case POSTTXTALL_OP:
            {
                int exit = 0;
                if (data.hdr.len > MaxMsgSize) {
                    setHeader(&reply.hdr, OP_MSG_TOOLONG, "");
                    setData(&reply.data, "", NULL, 0);
                    sendHeader(my_workp->connfd, &(reply.hdr));
                    break;
                }
                for (int i = 0; i < tpool->tab->size && exit == 0; i++) {
                    if (tpool->tab->list[i] != NULL) {
                        if (strncmp(tpool->tab->list[i]->user, my_workp->hdr.sender, strlen(my_workp->hdr.sender)) != 0) {//non voglio inviare messaggi a me stesso
                            //printf("Ciclo for...invio a %s\n",tpool->tab->list[i]->user);
                            //fflush(stdout);
                            setHeader(&reply.hdr, TXT_MESSAGE, my_workp->hdr.sender);
                            setData(&reply.data, tpool->tab->list[i]->user, data.buf, data.hdr.len);
                            if (sendTxt(tpool->tab->list[i], &reply, my_workp, tpool) == -1) {
                                setHeader(&reply.hdr, OP_FAIL, "");
                                sendHeader(my_workp->connfd, &(reply.hdr));
                                exit = 1;
                                break;
                            }
                        }
                        //collisioni nella hashmap
                        node_t curr = tpool->tab->list[i];
                        while (curr->next != NULL && exit == 0) {
                            if (strncmp(curr->next->user, my_workp->hdr.sender, strlen(my_workp->hdr.sender)) != 0) {
                                setHeader(&reply.hdr, TXT_MESSAGE, my_workp->hdr.sender);
                                setData(&reply.data, curr->next->user, data.buf, data.hdr.len);
                                if (sendTxt(curr->next, &reply, my_workp, tpool) == -1) {
                                    setHeader(&reply.hdr, OP_FAIL, "");
                                    sendHeader(my_workp->connfd, &(reply.hdr));
                                    exit = 1;
                                }
                            }
                            curr = curr->next;
                        }
                    }

                }
                if (exit == 0) {
                    setHeader(&reply.hdr, OP_OK, "");
                    sendHeader(my_workp->connfd, &(reply.hdr));
                }
                break;
            }
            case POSTFILE_OP:
            {
                printf("Preparazione invio file...\n");
                fflush(stdout);
                //SYSCALL(notused,epoll_ctl(epollfd,EPOLL_CTL_DEL,my_workp->connfd,NULL),"epoll_ctl_del in thread");
                node_t rcv = hash_search(tpool->tab, data.hdr.receiver, 0);
                if (hash_search(tpool->tab, my_workp->hdr.sender, 0) != NULL && rcv != NULL) {
                    //sendData dal client
                    message_data_t dataFile;
                    readData(my_workp->connfd, &dataFile);
                    if (MaxFileSize * 1000 >= dataFile.hdr.len) {
                        FILE *file;
                        struct stat st;
                        char *path = malloc(strlen(DirName) + 2 + (data.hdr.len));
                        memset(path, 0, strlen(DirName) + 2 + (data.hdr.len));
                        strncpy(path, DirName, strlen(DirName) - 1);
                        printf("Preparazione apertura file...\n");
                        fflush(stdout);
                        if (stat(path, &st) == -1) {
                            mkdir(path, 0700);
                        }
                        strncat(path, "/",1);
                        int i = (data.hdr.len) - 1;
                        while (i >= 0 && (data.buf[i]) != '/') {
                            i--;
                        }
                        i++;
                        strncat(path, data.buf + i,data.hdr.len);

                        if ((file = fopen(path, "w+")) == NULL) {
                            perror("fopen");
                            exit(EXIT_FAILURE);
                        }

                        if ((notused = fwrite(dataFile.buf, dataFile.hdr.len, 1, file)) <= 0) {
                            perror("fwrite");
                            exit(EXIT_FAILURE);
                        }
                        SYSCALL(notused, fclose(file), "fclose");
                        free(path);
                        setHeader(&reply.hdr, OP_OK, "");
                        sendHeader(my_workp->connfd, &(reply.hdr));


                    } else {
                        setHeader(&reply.hdr, OP_MSG_TOOLONG, "");
                        setData(&reply.data, "", NULL, 0);
                        sendHeader(my_workp->connfd, &(reply.hdr));
                    }
                    //	ev.events =  EPOLLIN | EPOLLOUT | EPOLLRDHUP |EPOLLET;
                    //	ev.data.fd = my_workp->connfd;
                    //	SYSCALL(notused,epoll_ctl(epollfd, EPOLL_CTL_ADD,
                    //			my_workp->connfd,&ev),"epoll_ctl");
                    if (dataFile.buf != NULL) {
                        free(dataFile.buf);
                        dataFile.buf = NULL;
                    }
                }
                break;
            }
            case GETPREVMSGS_OP:
            {
                printf("\t MESSAGGIIIII\n");
                //sendRequest(my_workp->connfd
                break;
            }
            default:
                printf("\tATTENZIONE CASE DEFAULT\n");
                break;
        }
        if ((data.buf != NULL)) { //&& !(my_workp->hdr.op == POSTTXTALL_OP) && !(my_workp->hdr.op == POSTTXT_OP)){
            free(data.buf);
            data.buf = NULL;
        }
        ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
        ev.data.fd = my_workp->connfd;
        SYSCALL(notused, epoll_ctl(epollfd, EPOLL_CTL_MOD,
                my_workp->connfd, &ev), "epoll_ctl");

        free(my_workp);
        printf("fine lavoro\n");
        fflush(stdout);
    }

}

int tpool_add_work(tpool_t tpool, long connfd, message_hdr_t hdr) {
    tpool_work_t *workp;
    pthread_mutex_lock(&tpool->q_lock);

    if (tpool->cur_q_size == tpool->max_q_size) {
        pthread_mutex_unlock(&tpool->q_lock);
        exit(EXIT_FAILURE);
    }

    while ((tpool->cur_q_size == tpool->max_q_size) && (!(tpool->shutdown || tpool->q_closed))) {
        pthread_cond_wait(&tpool->q_not_full, &tpool->q_lock);
    }

    if (tpool->shutdown || tpool->q_closed) {
        pthread_mutex_unlock(&tpool->q_lock);
        exit(EXIT_FAILURE);
    }

    workp = (tpool_work_t *) malloc(sizeof (tpool_work_t));
    workp->hdr = hdr;
    workp->connfd = connfd;
    workp->next = NULL;
    if (tpool->cur_q_size == 0) {
        tpool->q_tail = tpool->q_head = workp;
        pthread_cond_broadcast(&tpool->q_not_empty);
    } else {
        (tpool->q_tail)->next = workp;
        tpool->q_tail = workp;
    }
    tpool->cur_q_size++;
    pthread_mutex_unlock(&tpool->q_lock);
    return 1;

}

int tpool_destroy(tpool_t *tpool, int finish) {
    int i;
    tpool_work_t *cur_nodep;

    if (pthread_mutex_lock(&((*tpool)->q_lock)) != 0) {
        perror("mutex lock");
        exit(EXIT_FAILURE);
    }

    if ((*tpool)->q_closed || (*tpool)->shutdown) {
        if (pthread_mutex_unlock(&((*tpool)->q_lock)) != 0) {
            perror("mutex unlock");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    (*tpool)->q_closed = 1;

    if (finish == 1) {
        while ((*tpool)->cur_q_size != 0) {
            if (pthread_cond_wait(&((*tpool)->q_empty), &((*tpool)->q_lock)) != 0) {
                perror("cond wait");
                exit(EXIT_FAILURE);
            }
        }
    }

    (*tpool)->shutdown = 1;

    if (pthread_mutex_unlock(&((*tpool)->q_lock)) != 0) {
        perror("mutex unlock");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_broadcast(&((*tpool)->q_not_empty)) != 0) {
        perror("cond broadcast");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_broadcast(&((*tpool)->q_not_full)) != 0) {
        perror("cond broadcast");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < (*tpool)->num_threads; i++) {
        if (pthread_join((*tpool)->threads[i], NULL) != 0) {
            perror("join");
            exit(EXIT_FAILURE);
        }
    }

    //libero mutex
    if (pthread_mutex_destroy(&((*tpool)->q_lock))) {
        fprintf(stderr, "Could not destroy my_lock");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_destroy(&((*tpool)->q_not_empty))) {
        fprintf(stderr, "Could not destroy my_lock");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_destroy(&((*tpool)->q_not_full))) {
        fprintf(stderr, "Could not destroy my_lock");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_destroy(&((*tpool)->q_empty))) {
        fprintf(stderr, "Could not destroy my_lock");
        exit(EXIT_FAILURE);
    }

    free((*tpool)->threads);
    while ((*tpool)->q_head != NULL) {
        cur_nodep = (*tpool)->q_head;
        (*tpool)->q_head = (*tpool)->q_head->next;
        free(cur_nodep);
    }

    free((*tpool));

    return 0;
}

int tpool_search_fd(tpool_t tpool, long fd) {
    tpool_work_t *cur_nodep = tpool->q_head;

    while (cur_nodep != NULL) {
        if (cur_nodep->connfd == fd)
            return 1;
        cur_nodep = cur_nodep->next;
    }
    return 0;

}

int sendTxt(node_t rcv, message_t * reply, tpool_work_t *my_workp, tpool_t tpool) {

    int r = 0;
    SYSCALL(r, pthread_mutex_lock(&wMsgs_lock), "lock");

    int i = rcv->msgsToRead;
    if (i < tpool->tab->MaxHistMsgs) {
        rcv->msgs[i].hdr = reply->hdr;
        rcv->msgs[i].data.hdr = reply->data.hdr;
        rcv->msgs[i].data.buf = malloc(sizeof (char)*reply->data.hdr.len + 1);
        strncpy(rcv->msgs[i].data.buf, reply->data.buf, sizeof (char)*reply->data.hdr.len);
        rcv->msgsToRead++;
        pthread_mutex_unlock(&wMsgs_lock);
    } else {
        pthread_mutex_unlock(&wMsgs_lock);
        return -1;
    }
    return 0;
}
