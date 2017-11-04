#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <message.h>
#include <connections.h>
#include <config.h>

int fullRead(long fd,char *buf, int len);
int fullWrite(long fd,char *buf, int len);

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
	int ret=0;
	int ret2=0;
	int len;

	memset(hdr, 0, sizeof(message_hdr_t));
	
	len = sizeof(op_t);
	while((len != 0) && (ret = read(connfd,&(hdr->op), sizeof(op_t))) != 0) {
		if(ret == -1){
			if(errno == EINTR || errno == EAGAIN)
				continue;
                        fprintf(stderr,"Errore fd:%ld",connfd);
			perror("read header");
			return -1;
		}else{
		len-=ret;
		}
	}
	if((ret2 = fullRead(connfd,hdr->sender, sizeof(char)*(MAX_NAME_LENGTH+1))) < 0) {
		perror("read header2");
		return -1;
	}
	return ret+ret2;
}

int readData(long fd, message_data_t *data){
	int ret=0,ret2=0,ret3=0;
	ssize_t size;
	int len;

	//memset(data, 0, sizeof(message_data_t));

	if((ret = fullRead(fd,data->hdr.receiver,sizeof(char)*(MAX_NAME_LENGTH+1))) < 0) {
		perror("read data");
		return -1;
	}

	len = sizeof(int);
	while((len != 0) && (ret2 = read(fd,&(data->hdr.len), sizeof(int))) != 0) {
		if(ret2 == -1){
			if(errno == EINTR || errno == EAGAIN)
				continue;
			perror("read data2");
			return -1;
		}
		len-=ret2;
	}

	if(data->hdr.len>0){
		size = (data->hdr.len);//attenzione
		data->buf = (char*)malloc(sizeof(char)*size);
		memset(data->buf,0,size);

		if((ret3 = fullRead(fd,data->buf, size)) < 0) {
			perror("read data3");
			return -1;
		}

	}
	else data->buf = NULL;

	return ret+ret2+ret3;	
}

int readMsg(long fd, message_t *msg){
	int ret,ret2;

	if((ret = readHeader(fd,&msg->hdr))<0){
		perror("readheader in readmsg");
		return -1;
	}
	if((ret2 = readData(fd,&msg->data))<0){
		perror("readdata in readmsg");
		return -1;
	}
	return ret+ret2;
}

int sendHeader(long fd, message_hdr_t *hdr){
	int ret,ret2;
	int len;

	len = sizeof(op_t);
	while((len != 0) && (ret = write(fd,&(hdr->op),sizeof(op_t)))!=0){
		if(ret == -1){
			if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			perror("write in sendheader");
			fprintf(stderr,"fd: %ld\n",fd);
			return -1;
		}
		len-=ret;
	}
	
	if((ret2 = fullWrite(fd,hdr->sender,sizeof(char)*(MAX_NAME_LENGTH+1)))<0){
		perror("write2 in sendheader");
		return -1;
	}
	return ret+ret2;
}

int sendRequest(long fd, message_t *msg){

	int ret,ret2;
	
	if((ret = sendHeader(fd,&msg->hdr))<0){
		perror("sendreq");
		return -1;
	}
	if((ret2 = sendData(fd,&msg->data))<0){
		perror("sendreq2");
		return -1;
	}
	return ret+ret2;

}

int sendData(long fd, message_data_t *data){
	int ret,ret2,ret3;

	if((ret = fullWrite(fd,data->hdr.receiver,sizeof(char)*(MAX_NAME_LENGTH+1)))<0){
		perror("write in sendDATA");
		return -1;
	}
	int len = sizeof(int);
	while((len != 0) && (ret2 = write(fd,&data->hdr.len,sizeof(int)))!=0){
		if(ret2 == -1){
			if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			perror("write in sendDATA2");
			return -1;
		}
		len-=ret2;
	}

	if(data->hdr.len>0)
	{
		if((ret3 = fullWrite (fd, data->buf, sizeof(char)*(data->hdr.len)))<0){
			perror("write in sendadata");		
			return -1;
		}
	}

	return ret+ret2+ret3;
}

int fullRead(long fd,char *buf, int len){
	ssize_t ret;
	ssize_t tot=0;
	assert(fd > 0);
	while(len != 0 && (ret = read(fd,buf,len)) != 0) {
		if(ret == -1){
			if(errno == EINTR)
				continue;
			else if(errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			perror("full read");
			break;
		}
		len -= ret;
		buf += ret;
		tot += ret;
	}
	return tot;
}

int fullWrite(long fd,char *buf, int len){
	ssize_t ret;
	ssize_t wrote=0;

	while(len != 0 && (ret = write(fd,buf,len)) != 0) {
		if(ret == -1){
			if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			perror("full write");
			break;
		}
		len -= ret;
		buf += ret;
		wrote += ret;
	}
	return wrote;
}
