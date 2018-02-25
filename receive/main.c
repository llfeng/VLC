#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>    // for gettimeofday()
#include "tag.h"


#define INTERVAL 120


int tag_status = SLEEP;


int N = 10;
int T = 100;		//unit is ms

void update_interval(int *interval, int slave_count, int period){
	if(slave_count){
		*interval = period/slave_count;
	}else{
		*interval = 0;
	}
}

#define SERV_PORT	1234

int sockfd_array[8]; 
int start_udp_server(){
	int sockfd = 0;
	struct sockaddr_in servaddr, cliaddr; 
	int i = 0;
	for(i = 0; i < 8; i++){
		sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
		bzero(&servaddr, sizeof(servaddr)); 
		servaddr.sin_family = AF_INET; 
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
		servaddr.sin_port = htons(SERV_PORT+i); 
		if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) 
		{ 
			perror("bind error"); 
			exit(1); 
		} 
		sockfd_array[i] = sockfd;
	}
	return 0;
}

typedef enum{
	BEACON_TYPE = 0,
	REQ_TYPE,
	BEACON_RSP_TYPE,
	REQ_RSP_TYPE,
	RET_TYPE,
	RET_COLLSION_TYPE,
	DHCP_TYPE
} frame_type_t;

typedef struct{
	char type;
	char more_frag;
	char more_session;
	char addr;
	char payload_len;
	char *payload;
}frame_t;

/****** MAC FRAME *******

低速率限制协议的复杂度，也限制了网络的规模。

| type(3bit) | more fragment(1bit) | more session(1bit) | addr(3bit) | payload length(8bit) | payload | FCS(8bit) |

************************/


int gen_frame(char *frame_buf, frame_type_t type, char more_frag, char more_session, char addr, char payload_len,  char *payload){
#define HDR_LEN	3
	if(frame_buf){
		char sign = (type<<5);
		sign |= (more_frag<<4);
		sign |= (more_session<<3);
		sign |= addr;
		int index = 0;
		frame_buf[index++] = sign;
		frame_buf[index++] = payload_len;
		memcpy(&frame_buf[index], payload, payload_len);
		index += payload_len;
		int i = 0;
		char sum = 0;
		for(i = 0; i < index; i++){
			sum += frame_buf[i];
		}
		frame_buf[index++] = sum;
		return index;
	}else{
		return 0;
	}
}

frame_t *decode_frame(char *frame_buf, int buf_len, tag_t *vitag){
	int index = 0;
	int i = 0;
	char sum = 0;
	frame_t *frame = NULL;

	for(i = 0; i < buf_len-1; i++){
		sum += frame_buf[i];
	}
	if(sum == frame_buf[buf_len-1]){
		frame = (frame_t *)malloc(sizeof(frame_t));
		memset(frame, 0, sizeof(frame_t));
		frame->type = (frame_buf[0]&0xE0)>>5;
		frame->more_frag = (frame_buf[0]&0x10)>>4;
		frame->more_session = (frame_buf[0]&0x08)>>3;
		frame->addr = (frame_buf[0]&0x07);
		frame->payload_len = frame_buf[1];
		frame->payload = (char *)malloc(frame->payload_len);
		memcpy(frame->payload, &frame_buf[2], frame->payload_len);

#if 1
//		printf("frame->payload:%s\n", frame->payload);
		printf("sock[%d]--2--type:%d more_flag:%d more_session:%d addr:%d payload_len:%d\n", vitag->sockfd, frame->type,frame->more_frag, frame->more_session, frame->addr,frame->payload_len);
//		printf("decode success\n");
       
#endif
	}
	return frame;
}


#define PEER_PORT	1234
//#define HOST	"192.168.43.6"
//#define HOST	"255.255.255.255"
#define HOST "127.0.0.1"

#if 0
void xmit(){
	char *host = HOST;
	struct sockaddr_in peer_addr_array[8];
	memset(peer_addr_array, 0, sizeof(struct sockaddr_in));
	int i = 0;
	for(i = 0; i < 8; i++){
		peer_addr_array[i].sin_family = AF_INET;
		inet_pton(AF_INET, host, &peer_addr_array[i].sin_addr);
		peer_addr_array[i].sin_port = htons(PEER_PORT+i);
	}
	char *buf = "i am xmit";
	char frame_buf[64] = {0};
	gen_frame(frame_buf, REQ_TYPE, 0 , 0, strlen(buf),  buf);
	for(i = 0; i < 8; i++){
		sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&peer_addr_array[i], sizeof(struct sockaddr));
	}
}

void send_beacon(){
	char *host = HOST;
	struct sockaddr_in peer_addr;
	memset(&peer_addr, 0, sizeof(struct sockaddr_in));
	peer_addr.sin_family = AF_INET;
	inet_pton(AF_INET, host, &peer_addr.sin_addr);
	peer_addr.sin_port = htons(PEER_PORT);
	char *buf = "i am beacon";
	char frame_buf[64] = {0};
	gen_frame(frame_buf, REQ_TYPE, 0 , 0, strlen(buf),  buf);
	sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
}
#endif

#define MTU	64
#define SESSION_LEN	4		//unit is rtt


void send_beacon_rsp(tag_t *vitag, struct sockaddr_in *peer_addr){
	char frame_buf[64] = {0};
	char payload[1] = {0xAA};
	int buf_len = gen_frame(frame_buf, BEACON_RSP_TYPE, 0, 0, vitag->addr, 1,  payload);
	int n = sendto(vitag->sockfd, frame_buf, buf_len, 0, (struct sockaddr*)peer_addr, sizeof(struct sockaddr));
	vitag->status = DHCP;
	printf("sock[%d]---send beacon rsp\n", vitag->sockfd);
//	printf("send n=%d\n", n);
}



void parse_req(tag_t *vitag, frame_t *frame){
	memcpy(&vitag->next_req_time, frame->payload, sizeof(struct timeval));
}

void send_req_rsp(tag_t *vitag, struct sockaddr_in *peer_addr){
	char frame_buf[64] = {0};
	char payload[4] = {0xAA, 0xBB, 0xCC, 0xDD};
	printf("sock[%d]---send req rsp\n", vitag->sockfd);
	int buf_len = gen_frame(frame_buf, BEACON_RSP_TYPE, 0, 0, vitag->addr, sizeof(payload),  payload);
	sendto(vitag->sockfd, frame_buf, buf_len, 0, (struct sockaddr*)peer_addr, sizeof(struct sockaddr));
}

void down_speed(tag_t *vitag, struct sockaddr_in *peer_addr){
}

void hash_interval(tag_t *vitag, struct sockaddr_in *peer_addr){
}


void update_ip(tag_t *vitag, frame_t *frame){
	vitag->status = LISTEN;
    vitag->next_req_time = frame->payload[1] + time(NULL);
	vitag->addr = frame->addr;
}

int is_active(){
	return 1;
}

int handle_frame(tag_t *vitag, frame_t *frame, struct sockaddr_in *peer_addr){
	int ret = 0;
	if(frame){
		switch(frame->type){
			case BEACON_TYPE:
                if(vitag->status == ACTIVE){
                    send_beacon_rsp(vitag, peer_addr);
                }
				break;
			case REQ_TYPE:
				if(vitag->status == LISTEN && vitag->addr == frame->addr){
					parse_req(vitag, frame);
					send_req_rsp(vitag, peer_addr);
				}
				break;
			case RET_TYPE:
				down_speed(vitag, peer_addr);
				send_req_rsp(vitag, peer_addr);
				break;
			case RET_COLLSION_TYPE:
				hash_interval(vitag, peer_addr);
				break;
			case DHCP_TYPE:
                if(vitag->status == DHCP){
                    update_ip(vitag, frame);
                }
				break;
			default:
				break;
		}
	}else{
		ret = -1;
	}
	return ret;
}

void receive(tag_t *vitag){
#define MAXLINE	1024
	struct sockaddr_in peer_addr;
	memset(&peer_addr, 0, sizeof(struct sockaddr_in));
	socklen_t addrlen = sizeof(peer_addr);
	char buf[MAXLINE] = {0};
	int n = recvfrom(vitag->sockfd, buf, MAXLINE, 0, (struct sockaddr*)&peer_addr, &addrlen);	
	struct timeval recv_time;
	memset(&recv_time, 0, sizeof(struct timeval));
	if(n>0){
		gettimeofday( &recv_time, NULL );
		frame_t *frame = decode_frame(buf, n, vitag);
		printf("sock[%d]--1---reveive len=%d timestamp:%ld.%ld\n", vitag->sockfd, n, recv_time.tv_sec, recv_time.tv_usec);
		handle_frame(vitag,frame, &peer_addr);
#if 0
		free(frame->payload);
		frame->payload = NULL;
		free(frame);
		frame = NULL;
#endif
	}
}	


int timeslot_vaild(tag_t *vitag, int app_tick){
    int ret = 0;
	if(vitag->status == LISTEN){
		if((app_tick - vitag->next_req_time) / INTERVAL == 0){          
			ret = 1;
		}else{
			vitag->status = ACTIVE;
			ret = 0;
		}
	}else if(vitag->status == ACTIVE || vitag->status == DHCP){
		if((time(NULL) - app_tick)%120/15 == 0){
			ret = 1;
		}else{
			ret = 0;
		}
	}
    return ret;
}

void *listen_reader(void *para){
	tag_t *vitag = (tag_t *)malloc(sizeof(tag_t));
	memset(vitag, 0, sizeof(tag_t));
	vitag->sockfd = *((int *)para);
	vitag->status = SLEEP;
	struct timeval seed_time;
	memset(&seed_time, 0, sizeof(struct timeval));
	gettimeofday( &seed_time, NULL );
	srand(seed_time.tv_usec);
	int start_time = rand()%120;
    printf("sockfd:%d i will sleep %ds\n", vitag->sockfd, start_time);
	sleep(start_time);
    printf("sockfd:%d i have awake\n", vitag->sockfd);
	vitag->status = ACTIVE;
	int app_tick;
	while(1){
		struct timeval tv;
		fd_set readfds;
		int i=0;
		unsigned int n=0;
        app_tick = time(NULL);
		if(timeslot_vaild(vitag, app_tick)){
			FD_ZERO(&readfds);
			FD_SET(vitag->sockfd,&readfds);
			tv.tv_sec=0;
			tv.tv_usec=100000;
			select(vitag->sockfd+1,&readfds,NULL,NULL,&tv);
			if(FD_ISSET(vitag->sockfd,&readfds)){
				receive(vitag);
			}
		}
	}
}

int start_work_thread(){
	int i = 0;
	pthread_t tid[8] = {0};
	for(i = 0; i < 8; i++){
		pthread_create(&tid[i], NULL, listen_reader, (void *)&sockfd_array[i]);
	}
	return 0;
}

int main(){

	start_udp_server();
	start_work_thread();
	while(1){
	}
}
