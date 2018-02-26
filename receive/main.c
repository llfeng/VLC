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

pthread_mutex_t port_mutex;

uint32_t g_port_index = 0;

uint32_t tag_status = SLEEP;


uint32_t N = 10;
uint32_t T = 100;		//unit is ms

void update_interval(uint32_t *interval, uint32_t slave_count, uint32_t period){
	if(slave_count){
		*interval = period/slave_count;
	}else{
		*interval = 0;
	}
}

#define SERV_PORT	1234

uint32_t sockfd_array[8]; 
uint32_t start_udp_server(){
	uint32_t sockfd = 0;
	struct sockaddr_in servaddr, cliaddr; 
	uint32_t i = 0;
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
	uint8_t type;
	uint8_t more_frag;
	uint8_t more_session;
	uint8_t addr;
    uint8_t next_req_time;
	uint8_t payload_len;
	uint8_t *payload;
}frame_t;

/****** MAC FRAME *******

低速率限制协议的复杂度，也限制了网络的规模。

| type(3bit) | more fragment(1bit) | more session(1bit) | addr(3bit) | next req time(8bit) | payload length(8bit) | payload | FCS(8bit) |

************************/


uint32_t gen_frame(uint8_t *frame_buf, frame_type_t type, uint8_t more_frag, uint8_t more_session, uint8_t addr, uint8_t next_req_time, uint8_t payload_len,  uint8_t *payload){
#define HDR_LEN	3
	if(frame_buf){
		uint8_t sign = (type<<5);
		sign |= (more_frag<<4);
		sign |= (more_session<<3);
		sign |= addr;
		uint32_t index = 0;
		frame_buf[index++] = sign;
        frame_buf[index++] = next_req_time;
		frame_buf[index++] = payload_len;      
		memcpy(&frame_buf[index], payload, payload_len);
		index += payload_len;
		uint32_t i = 0;
		uint8_t sum = 0;
		for(i = 0; i < index; i++){
			sum += frame_buf[i];
		}
		frame_buf[index++] = sum;
		return index;
	}else{
		return 0;
	}
}

frame_t *decode_frame(uint8_t *frame_buf, uint32_t buf_len, tag_t *vitag){
	uint32_t index = 0;
	uint32_t i = 0;
	uint8_t sum = 0;
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
        frame->next_req_time = frame_buf[1];
		frame->payload_len = frame_buf[2];
        if(frame->payload_len){
            frame->payload = (uint8_t *)malloc(frame->payload_len);
            memcpy(frame->payload, &frame_buf[3], frame->payload_len);
        }

#if 1
//		printf("frame->payload:%s\n", frame->payload);
//		printf("sock[%d]--2--type:%d more_flag:%d more_session:%d addr:%d next_req_time:%d payload_len:%d\n", vitag->sockfd, frame->type,frame->more_frag, frame->more_session, frame->addr, frame->next_req_time, frame->payload_len);
//		printf("decode success\n");
       
#endif
	}
	return frame;
}


#define PEER_PORT	1234
#define HOST "127.0.0.1"


#define MTU	64
#define SESSION_LEN	4		//unit is rtt


void send_beacon_rsp(tag_t *vitag, struct sockaddr_in *peer_addr){
	uint8_t frame_buf[64] = {0};
	uint8_t payload[1] = {0xAA};
	uint32_t buf_len = gen_frame(frame_buf, BEACON_RSP_TYPE, 0, 0, vitag->addr, 0,  1,  payload);
	uint32_t n = sendto(vitag->sockfd, frame_buf, buf_len, 0, (struct sockaddr*)peer_addr, sizeof(struct sockaddr));
	vitag->status = DHCP;
	printf("sock[%d]---send beacon rsp\n", vitag->sockfd);
}



void parse_req(tag_t *vitag, frame_t *frame){
	memcpy(&vitag->next_req_time, frame->payload, sizeof(struct timeval));
}

void send_req_rsp(tag_t *vitag, struct sockaddr_in *peer_addr){
	uint8_t frame_buf[64] = {0};
	uint8_t payload[4] = {0xAA, 0xBB, 0xCC, 0xDD};
	printf("sock[%d]---send req rsp\n", vitag->sockfd);
	uint32_t buf_len = gen_frame(frame_buf, BEACON_RSP_TYPE, 0, 0, vitag->addr, 0, sizeof(payload),  payload);
	sendto(vitag->sockfd, frame_buf, buf_len, 0, (struct sockaddr*)peer_addr, sizeof(struct sockaddr));
}

void down_speed(tag_t *vitag, struct sockaddr_in *peer_addr){
}

void hash_interval(tag_t *vitag, struct sockaddr_in *peer_addr){
}


void update_ip(tag_t *vitag, frame_t *frame){
	vitag->status = LISTEN;
    vitag->next_req_time = frame->next_req_time;
	vitag->addr = frame->addr;
}

uint32_t is_active(){
	return 1;
}

int32_t handle_frame(tag_t *vitag, frame_t *frame, struct sockaddr_in *peer_addr){
	int32_t ret = 0;
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
	uint8_t buf[MAXLINE] = {0};
	uint32_t n = recvfrom(vitag->sockfd, buf, MAXLINE, 0, (struct sockaddr*)&peer_addr, &addrlen);	
	struct timeval recv_time;
	memset(&recv_time, 0, sizeof(struct timeval));
	if(n>0){
		gettimeofday( &recv_time, NULL );
		frame_t *frame = NULL;
        frame = decode_frame(buf, n, vitag);
        if(frame){
            printf("sock[%d]---type:%d more_flag:%d more_session:%d addr:%d next_req_time:%d payload_len:%d reveive len=%d timestamp:%ld.%ld\n", vitag->sockfd, frame->type,frame->more_frag, frame->more_session, frame->addr, frame->next_req_time, frame->payload_len, n, recv_time.tv_sec, recv_time.tv_usec);
            handle_frame(vitag,frame, &peer_addr);
            if(frame){
                if(frame->payload){
                    free(frame->payload);
                    frame->payload = NULL;
                }
                free(frame);
                frame = NULL;
            }
        }else{
            printf("malloc frame err\n");
        }
    }
}	


uint32_t timeslot_vaild(tag_t *vitag, uint32_t app_tick){
    uint32_t ret = 0;
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
//	vitag->sockfd = *((uint32_t *)para);
	vitag->status = SLEEP;
	struct timeval seed_time;
	memset(&seed_time, 0, sizeof(struct timeval));
	gettimeofday( &seed_time, NULL );
	srand(seed_time.tv_usec);
	uint32_t start_time = rand()%120;
    printf("sockfd:%d i will sleep %ds\n", vitag->sockfd, start_time);
	sleep(start_time);
    printf("sockfd:%d i have awake\n", vitag->sockfd);


	uint32_t sockfd = 0;
	struct sockaddr_in servaddr, cliaddr; 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
    bzero(&servaddr, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    pthread_mutex_lock(&port_mutex);
    servaddr.sin_port = htons(SERV_PORT+g_port_index++); 
    pthread_mutex_unlock(&port_mutex);
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) 
    { 
        perror("bind error"); 
        exit(1); 
    } 
	vitag->sockfd = sockfd;


    vitag->status = ACTIVE;
    uint32_t app_tick;
    while(1){
		struct timeval tv;
		fd_set readfds;
		uint32_t i=0;
		uint32_t n=0;
        app_tick = time(NULL);
//		if(timeslot_vaild(vitag, app_tick)){
			FD_ZERO(&readfds);
			FD_SET(vitag->sockfd,&readfds);
			tv.tv_sec=0;
			tv.tv_usec=100000;
			select(vitag->sockfd+1,&readfds,NULL,NULL,&tv);
			if(FD_ISSET(vitag->sockfd,&readfds)){
				receive(vitag);
			}
//		}
	}
}

uint32_t start_work_thread(){
	uint32_t i = 0;
	pthread_t tid[8] = {0};
	for(i = 0; i < 8; i++){
		pthread_create(&tid[i], NULL, listen_reader, NULL);
	}
	return 0;
}

uint32_t main(){
    pthread_mutex_init(&port_mutex, NULL);
//	start_udp_server();
	start_work_thread();
	while(1){
	}
}
