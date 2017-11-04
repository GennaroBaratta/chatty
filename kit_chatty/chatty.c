/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>


/* inserire gli altri include che servono */
#include <stats.h>
#include <pthread.h>
#include <tpool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <parsing.h>
#include <connections.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define SYSCALL(r,c,e) \
    if((r=c)==-1) { perror(e);exit(errno); }

tpool_t thread_pool;
hashmap_t tab;

/* struttura che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h.
 *
 */
struct statistics chattyStats = {0, 0, 0, 0, 0, 0, 0};

//configurazione del server
char * UnixPath = NULL;
char * DirName = NULL;
char * StatFileName = NULL;
int MaxConnections = 0;
int ThreadsInPool = 0;
int MaxMsgSize = 0;
int MaxFileSize = 0;
int MaxHistMsgs = 0;
int epollfd, nfds;
struct epoll_event ev;
volatile sig_atomic_t KillAll = 1;
int printstat = 0;

pthread_mutex_t *fd_lock = NULL;
pthread_mutex_t wMsgs_lock = PTHREAD_MUTEX_INITIALIZER;

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

static void killall(int signum) {
    KillAll = 0;
}

static void print(int signum) {
    printstat = 1;
}

void setnonblocking(int sock) {
    int opts;
    if ((opts = fcntl(sock, F_GETFL)) < 0)
        perror("GETFL failed");
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0)
        perror("SETFL  failed");
}

int main(int argc, char *argv[]) {
    //Controllo argomenti in linea di comando
    if ((argc != 3) || ((strncmp(argv[1], "-f", 2) != 0))) {
        usage(argv[0]);
        return 0;
    }

    //puntatore al file di configurazione
    FILE * fp;
    char conf[MAXLINE];
    //"./DATA/chatty.conf1"
    strcpy(conf, argv[2]);
    if ((fp = fopen(conf, "r")) != NULL) {
        //Call alla funzione che parsa il file di configurazione
        parsing(fp);
    } else {
        perror("open");
        exit(EXIT_FAILURE);
    }
    if (fclose(fp) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    //clean di eventuali connesioni precedenti
    unlink(UnixPath);

    //array di lock per ogni connesione
    fd_lock = (pthread_mutex_t*) malloc((MaxConnections) * sizeof (pthread_mutex_t));
    for (int i = 0; i < MaxConnections; i++) {
        pthread_mutex_init(&fd_lock[i], NULL);
    }
    //Debug configuazione server
    printf("up:%s dir:%s stats:%s mf:%d mtxt:%d %d %d %d\n", UnixPath, DirName, StatFileName, MaxFileSize, MaxMsgSize, ThreadsInPool, MaxConnections, MaxHistMsgs);
    fflush(stdout);


    //Inizializzazione tabella hash
    if (hash_init(&tab, 100, MaxHistMsgs) != 0) {
        perror("hash_init");
        exit(EXIT_FAILURE);
    }
    //Inizializzazione pool di thread
    tpool_init(&thread_pool, ThreadsInPool, MaxConnections * 2,
            tab, MaxConnections);

    int listenfd;
    SYSCALL(listenfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof (serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, UnixPath, strlen(UnixPath) + 1);
    int notused = 0;
    SYSCALL(notused, bind(listenfd, (struct sockaddr*) &serv_addr,
            sizeof (serv_addr)), "bind");
    setnonblocking(listenfd);
    SYSCALL(notused, listen(listenfd, MaxConnections), "listen");

    //Gestione segnali
    struct sigaction s;
    memset(&s, 0, sizeof (s));
    s.sa_handler = killall;
    SYSCALL(notused, sigaction(SIGINT, &s, NULL), "sigact");

    struct sigaction spipe;
    memset(&spipe, 0, sizeof (spipe));
    spipe.sa_handler = SIG_IGN;
    SYSCALL(notused, sigaction(SIGPIPE, &spipe, NULL), "sigact");

    struct sigaction stat;
    memset(&stat, 0, sizeof (stat));
    stat.sa_handler = print;
    SYSCALL(notused, sigaction(SIGUSR1, &stat, NULL), "sigacta");


    struct epoll_event events[MaxConnections * 2];

    SYSCALL(epollfd, epoll_create1(0), "epoll_create");
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    SYSCALL(notused, epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev),
            "epoll_ctl:listenfd");

    message_hdr_t hdr[MaxConnections + 20];
    for (int i = 0; i < MaxConnections + 20; i++) {
        hdr[i].op = -1;
    }


    while (KillAll) {
        //printf("waiting...");
        fflush(stdout);

        //stampa statistiche
        if (printstat == 1) {
            FILE * fdstat;
            if ((fdstat = fopen(StatFileName, "a")) == NULL) {
                perror("fopen stat");
                exit(EXIT_FAILURE);
            }
            printStats(fdstat);
            printstat = 0;
            SYSCALL(notused, fclose(fdstat), "fclose stat");
        }


        if ((nfds = epoll_wait(epollfd, events, MaxConnections, -1)) == -1) {
            if (errno != EINTR) {
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            }
            break;
        }

        long connfd;
        //printf("%d\t",nfds);
        fflush(stdout);
        for (int i = 0; i < nfds; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (events[i].events & EPOLLRDHUP)) {
                //ERRORE
                hash_status(tab, hdr[events[i].data.fd].sender, 0); //offline
                printf("Disconnesso %s\n", hdr[events[i].data.fd].sender);
                fflush(stdout);
                SYSCALL(notused, epoll_ctl(epollfd, EPOLL_CTL_DEL,
                        events[i].data.fd, &ev), "epoll_ctl_del");
                SYSCALL(notused, close(events[i].data.fd), "close");
            } else if (events[i].data.fd == listenfd) {
                //server socket, ciclo di accept
                while (1) {
                    connfd = accept(listenfd, (struct sockaddr*) NULL, NULL);
                    if (connfd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            //tutte le connessioni processate
                            break;
                        } else {
                            perror("accept");
                            exit(EXIT_FAILURE);
                        }
                    } else {
                        printf("connection accepted\n");
                        fflush(stdout);
                        setnonblocking(connfd);
                        ev.data.fd = connfd;
                        ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
                        SYSCALL(notused, epoll_ctl(epollfd, EPOLL_CTL_ADD,
                                connfd, &ev), "epoll_ctl_add")
                    }
                }
            } else {
                //socket cliente
                if (events[i].events & EPOLLIN) {
                    if (events[i].data.fd < 0)
                        continue;
                    message_hdr_t temp_hdr;

                    if (readHeader(events[i].data.fd, &temp_hdr) > 0) {
                        hdr[events[i].data.fd] = temp_hdr;
                        printf("work added, fd: %d user:%s op:%d\n", events[i].data.fd, hdr[events[i].data.fd].sender, hdr[events[i].data.fd].op);
                        fflush(stdout);
                        tpool_add_work(thread_pool, events[i].data.fd, hdr[events[i].data.fd]);
                    }
                } 
            }
        }

    }
    close(epollfd);
    unlink(UnixPath);
    free(UnixPath);
    free(DirName);
    for (int i = 0; i < MaxConnections; i++) {
        pthread_mutex_destroy(&fd_lock[i]);
    }
    free(fd_lock);
    free(StatFileName);
    tpool_destroy(&thread_pool, 1);
    hash_free(&tab);
    printf("Tutto chiuso\n");
    fflush(stdout);


    return 0;
}
