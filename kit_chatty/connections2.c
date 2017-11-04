#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <message.h>
#include <connections.h>

int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	struct sockaddr_un serv_addr;
	int sockfd;
	if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) <= 0){
		perror("socket");
		return -1;
	}
	memset(&serv_addr, '0', sizeof(serv_addr));
	
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path,path, strlen(path)+1);
	

	while(ntimes>0){
		if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1){
			perror("connect");
			fprintf(stderr, "ERRORE: tentativo di connessione...\n");
			ntimes--;
			sleep(secs);
		}else{
			return sockfd;
		}
	}   
	return -1;
}

int readHeader(long connfd, message_hdr_t *hdr){
	size_t left=sizeof(op_t);
	size_t r;
	size_t tot=0;
	memset(hdr,0,sizeof(message_hdr_t));

	while(left > 0){
		if((r=read((int)connfd, &(hdr->op),left)) == -1 ){
			if(errno == EINTR)continue;
			return -1;
		}
		if(r == 0) return 0;
		left -= r;
		tot += r;
	}


	left = sizeof(char)*(MAX_NAME_LENGTH+1);
	char *buf = hdr->sender;

	while(left > 0) {
		if ((r=read((int) connfd, buf, left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
		buf += r;
		tot += r;
	}
	
	return r;

}

int readData(long fd, message_data_t *data){
	size_t left;
	int r;
	char *bufptr;
	
	left = sizeof(message_data_hdr_t);
	memset(data, 0, left);
	
	
	while(left > 0) {
		if ((r=read((int) fd, &(data->hdr), left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
	}
	left = data->hdr.len*sizeof(char);
	data->buf = malloc(left);
	bufptr=data->buf;
	while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
    left    -= r;
	bufptr  += r;
    }

	return 1;
}

int readMsg(long fd, message_t *msg){
	size_t left=sizeof(message_hdr_t);
	int r;
	char *bufptr = msg->data.buf;

	while(left > 0) {
		if ((r=recv((int) fd, &(msg->hdr), left,0)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
	}

	left = sizeof(message_data_hdr_t);
	while(left > 0) {
		if ((r=read((int) fd, &(msg->data.hdr), left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
	}
	left = msg->data.hdr.len;
	while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }

	return 1;	
	
	
}

int sendReply(long fd, message_hdr_t *hdr){
	size_t left=sizeof(message_hdr_t);
	int r;

	while(left > 0) {
		if ((r=write((int) fd, hdr, left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
	}
	dprintf(fd,"mandato\n");
	return r;	
}

int sendRequest(long fd, message_t *msg){
	size_t left=sizeof(message_hdr_t);
	int r;
	char *bufptr = msg->data.buf;

	while(left > 0) {
		if ((r=write((int) fd, &(msg->hdr), left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
	}

	left = sizeof(message_data_hdr_t);
	while(left > 0) {
		if ((r=write((int) fd, &(msg->data.hdr), left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
	}
	left = msg->data.hdr.len;
	while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }

	return 1;	
}

int sendData(long fd, message_data_t *data){
	size_t left;
	int r;
	char *bufptr = data->buf;
	
	left = sizeof(message_data_hdr_t);
	while(left > 0) {
		if ((r=write((int) fd, &(data->hdr), left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if ( r == 0) return 0;
		left -= r;
	}
	left = (data->hdr.len)*sizeof(char);
	while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }
	return 1;
	


}