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
#include <sys/time.h>
#include <time.h>

typedef struct{
	char vaild;
	char addr;
	char rsp_flag;
}addr_t;

addr_t addr_table[8];


int tag_count = 0;

int N = 10;
int T = 100;		//unit is ms

#define MAX_COUNT	8
#define INTERVAL	15		//unit is sec
#define PERIOD		120		//unit is sec

void update_interval(int *interval, int slave_count, int period){
	if(slave_count){
		*interval = period/slave_count;
	}else{
		*interval = 0;
	}
}

#define SERV_PORT	8888

int sockfd; 
int start_udp_server(){
	struct sockaddr_in servaddr, cliaddr; 
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	servaddr.sin_port = htons(SERV_PORT); 
	if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) 
	{ 
		perror("bind error"); 
		exit(1); 
	} 
	return sockfd;
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


int gen_frame(char *frame_buf, frame_type_t type, char more_frag, char more_session, char addr,  char payload_len,  char *payload){
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
		return index;;
	}else{
		return -1;
	}
}

#define PEER_PORT	1234
#define HOST "127.0.0.1"

void send_req(char addr, char *data, char data_len){
	char *host = HOST;
	struct sockaddr_in peer_addr_array[8];
	memset(peer_addr_array, 0, sizeof(struct sockaddr_in));
	int i = 0;
	for(i = 0; i < 8; i++){
		peer_addr_array[i].sin_family = AF_INET;
		inet_pton(AF_INET, host, &peer_addr_array[i].sin_addr);
		peer_addr_array[i].sin_port = htons(PEER_PORT+i);
	}
//	char *buf = "i am xmit";

	char frame_buf[64] = {0};
	int buf_len = gen_frame(frame_buf, REQ_TYPE, 0 , 0, 0,  data_len,  data);
	for(i = 0; i < 8; i++){
		int ret = sendto(sockfd, frame_buf, buf_len, 0, (struct sockaddr*)&peer_addr_array[i], sizeof(struct sockaddr));
		printf("send[%d] ret=%d\n", i, ret);
	}
}

void send_beacon(){
	char *host = HOST;
	struct sockaddr_in peer_addr_array[8];
	memset(peer_addr_array, 0, sizeof(struct sockaddr_in));
	int i = 0;
	for(i = 0; i < 8; i++){
		peer_addr_array[i].sin_family = AF_INET;
		inet_pton(AF_INET, host, &peer_addr_array[i].sin_addr);
		peer_addr_array[i].sin_port = htons(PEER_PORT+i);
	}
	char *buf = "i am beacon";
	char frame_buf[64] = {0};
	int buf_len = gen_frame(frame_buf, BEACON_TYPE, 0 , 0, 0,  strlen(buf),  buf);
	for(i = 0; i < 8; i++){
		int ret = sendto(sockfd, frame_buf, buf_len, 0, (struct sockaddr*)&peer_addr_array[i], sizeof(struct sockaddr));
		printf("send[%d] ret=%d\n", i, ret);
	}
}


#define MTU	64
#define SESSION_LEN	4	//unit is rtt

frame_t *decode_frame(char *frame_buf, int buf_len){
	int index = 0;
    int i = 0;
    char sum = 0;
    frame_t *frame = NULL;

	printf("frame buf: ");
    for(i = 0; i < buf_len-1; i++){
		printf("%2d ", frame_buf[i]);
        sum += frame_buf[i];
    }
	printf("%2d\n", frame_buf[buf_len]);
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
        printf("type:%d more_flag:%d more_session:%d addr:%d payload_len:%d\n", frame->type,frame->more_frag, frame->more_session, frame->addr,frame->payload_len);

        printf("decode success\n");
    }
    return frame;
}

int check_beacon_rsp(char *payload, int len){
	char beacon_rsp_data[] = {0XAA};
	if(memcmp(beacon_rsp_data, payload, len) == 0){
		return 1;
	}else{
		return 0;
	}
}

int check_timesolt_vaild(){
	if(time(NULL)%10 < 9){
		return 1;
	}else{
		return 0;
	}
}

char find_vaild_ip(){
	int i = 0;
	for(i = 0; i < 8; i++){
		if(addr_table[i].vaild == 0){
			return i;
		}
	}
	return -1;
}


int check_timeslot_vaild(int recv_time){
	int ret = 0;
	if(INTERVAL - (recv_time%INTERVAL) > 2){
		ret = 1;
	}else{
		ret = 0;
	}
	return ret;
}

void cal_next_req_time(struct timeval cur_time, char addr){
    struct timeval start_time;
    memset(&start_time, 0, sizeof(struct timeval));

}


void send_dhcp(){
	printf("%s\n", __func__);
    int recv_time = time(NULL);
	if(check_timeslot_vaild(recv_time)){
		char frame_buf[64] = {0};
		char data[32] = {0};
		char addr = find_vaild_ip();
		int buf_len = 0;
		if(addr >= 0){
			char *host = HOST;
			struct sockaddr_in peer_addr_array[8];
			memset(peer_addr_array, 0, sizeof(struct sockaddr_in));
			int i = 0;
			for(i = 0; i < 8; i++){
				peer_addr_array[i].sin_family = AF_INET;
				inet_pton(AF_INET, host, &peer_addr_array[i].sin_addr);
				peer_addr_array[i].sin_port = htons(PEER_PORT+i);
			}
            char next_req_time = cal_next_time(addr);
			data[0] = next_req_time;
			int data_len = 1;
			buf_len = gen_frame(frame_buf, DHCP_TYPE, 0 , 0, addr, data_len,  data);
			for(i = 0; i < 8; i++){
				int ret = sendto(sockfd, frame_buf, buf_len, 0, (struct sockaddr*)&peer_addr_array[i], sizeof(struct sockaddr));
				printf("send[%d] ret=%d\n", i, ret);
			}
			addr_table[addr].vaild = 1;
			addr_table[addr].rsp_flag = 0;
			tag_count++;
		}
	}
}
void handle_frame(frame_t *frame){
	switch(frame->type){
		case BEACON_RSP_TYPE:
			if(frame->payload){
				if(check_beacon_rsp(frame->payload, frame->payload_len)){
					send_dhcp();
				}
			}
			break;
		case REQ_RSP_TYPE:
//			forward_to_app();
			break;
		default:
			break;
	}
}

void receive(){
#define MAXLINE	1024
	struct sockaddr_in peer_addr;
	memset(&peer_addr, 0, sizeof(struct sockaddr_in));
	socklen_t addrlen = sizeof(peer_addr);
	char buf[MAXLINE] = {0};
	int n = recvfrom(sockfd, buf, MAXLINE, 0, (struct sockaddr*)&peer_addr, &addrlen);	
	printf("receive len:%d\n", n);
	frame_t *frame = decode_frame(buf, n);
	if(frame){
		handle_frame(frame);	
	}
}

#if 0
void send_thread(){
	while(1){
		int app_tick = time(NULL);
		if(app_tick%10 < 9){
			if(app_tick%10 == 0 && tag_count){
				index = (app_tick%80)/10;
				if(addr_table[index].vaild){
					send_req(addr_table[index].addr, data, data_len);
				}else{
					send_beacon();
				}
			}else{
				send_beacon();
			}
		}else{
			//wait for receive
		}
		sleep(1);
	}
}
#endif

void init_addr_table(){
	int i = 0;
	for(i = 0; i < 8; i++){
		addr_table[i].vaild = 0;
		addr_table[i].addr = -1;
		addr_table[i].rsp_flag = 0;
	}
}




int main(){

	int timer = 0;
	int interval = INTERVAL;
	int app_tick = T;
	start_udp_server();
	if(sockfd < 0){
		return -1;
	}
	init_addr_table();
	while(1){
        int cur_time = time(NULL);
		if(check_timeslot_vaild(cur_time)){
			if(tag_count){
				int index = cur_time%120/INTERVAL;
                if(index != 0){
                    addr_table[index-1].rsp_flag = 0;
                }else{
                    addr_table[7].rsp_flag = 0;
                }
				if(addr_table[index].vaild && addr_table[index].rsp_flag == 0){
					char *data = "i am req";
					int data_len = strlen(data);
					send_req(addr_table[index].addr, data, data_len);
				}else{
					send_beacon();
				}
			}else{
				send_beacon();
			}
		}else{
			//wait for receive
		}
		struct timeval tv;
		fd_set readfds;
		int i=0;
		unsigned int n=0;
		FD_ZERO(&readfds);
		FD_SET(sockfd,&readfds);
		tv.tv_sec=1;
		tv.tv_usec=0;
		select(sockfd+1,&readfds,NULL,NULL,&tv);
		if(FD_ISSET(sockfd,&readfds))
		{
			receive();
		}
		app_tick++;
	}
}
