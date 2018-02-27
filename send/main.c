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
#include "scheduler.h"

typedef struct{
	uint8_t vaild;
	uint8_t addr;
	uint8_t rsp_flag;
}addr_t;

addr_t addr_table[8];



scheduler_t *scheduler = NULL;


uint32_t tag_count = 0;

uint32_t N = 10;
uint32_t T = 100;		//unit is ms

#define MAX_COUNT	8
#define INTERVAL	15		//unit is sec
#define PERIOD		120		//unit is sec

void update_interval(uint32_t *interval, uint32_t slave_count, uint32_t period){
	if(slave_count){
		*interval = period/slave_count;
	}else{
		*interval = 0;
	}
}

#define SERV_PORT	8888

uint32_t sockfd; 
uint32_t start_udp_server(){
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
    uint8_t type;
    uint8_t more_frag;
    uint8_t more_session;
    uint8_t addr;
    uint8_t payload_len;
    uint8_t *payload;
}frame_t;




//|---------PERIOD-----------|
//|-INTERVAL-|
//|-IFS-|
uint32_t cal_remain_tick(uint8_t addr){
    uint32_t remain_tick = 0;   
    uint32_t cur_period = app_tick/PERIOD;
    uint32_t next_period = cur_period + 1;
    
    uint32_t next_interval = addr;
    uint32_t cur_interval = app_tick%PERIOD/INTERVAL;

    uint32_t cur_ifs = app_tick%INTERVAL;


    if(next_interval <= cur_interval){
        uint32_t next_tick = (next_period*PERIOD) + (next_interval*INTERVAL);
        remain_tick = next_tick - app_tick;
    }else{
        uint32_t next_tick = cur_period*PERIOD + next_interval*INTERVAL;
        remain_tick = next_tick - app_tick;
    }   
    return remain_tick;
}




/****** MAC FRAME *******

低速率限制协议的复杂度，也限制了网络的规模。

| type(3bit) | more fragment(1bit) | more session(1bit) | addr(3bit) | next req time(8bit) | payload length(8bit) | payload | FCS(8bit) |

************************/


int32_t gen_frame(uint8_t *frame_buf, frame_type_t type, uint8_t more_frag, uint8_t more_session, uint8_t addr, uint8_t next_req_time, uint8_t payload_len,  uint8_t *payload){
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
        if(payload_len){
            memcpy(&frame_buf[index], payload, payload_len);
            index += payload_len;
        }
		uint32_t i = 0;
		uint8_t sum = 0;
		for(i = 0; i < index; i++){
			sum += frame_buf[i];
		}
		frame_buf[index++] = sum;
		return index;;
	}else{
		return -1;
	}
}


frame_t *decode_frame(uint8_t *frame_buf, uint32_t buf_len){
	uint32_t index = 0;
    uint32_t i = 0;
    uint8_t sum = 0;
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
        frame->payload_len = frame_buf[2];
        frame->payload = (uint8_t *)malloc(frame->payload_len);
        memcpy(frame->payload, &frame_buf[3], frame->payload_len);
        printf("type:%d more_flag:%d more_session:%d addr:%d payload_len:%d\n", frame->type,frame->more_frag, frame->more_session, frame->addr,frame->payload_len);

        printf("decode success\n");
    }
    return frame;
}



#define PEER_PORT	1234
#define HOST "127.0.0.1"

void send_req(uint8_t addr, uint8_t *data, uint8_t data_len){
	uint8_t *host = HOST;
	struct sockaddr_in peer_addr_array[8];
	memset(peer_addr_array, 0, sizeof(struct sockaddr_in));
	uint32_t i = 0;
	for(i = 0; i < 8; i++){
		peer_addr_array[i].sin_family = AF_INET;
		inet_pton(AF_INET, host, &peer_addr_array[i].sin_addr);
		peer_addr_array[i].sin_port = htons(PEER_PORT+i);
	}

	uint32_t remain_tick = cal_remain_tick(addr);

	uint8_t remain_tick_format = remain_tick/INTERVAL;
	remain_tick_format <<= 5;
	remain_tick_format |= remain_tick%INTERVAL;   


	uint8_t frame_buf[64] = {0};
	uint32_t buf_len = gen_frame(frame_buf, REQ_TYPE, 0 , 0, addr, remain_tick_format,  data_len,  data);
	for(i = 0; i < 8; i++){
		uint32_t ret = sendto(sockfd, frame_buf, buf_len, 0, (struct sockaddr*)&peer_addr_array[i], sizeof(struct sockaddr));
        frame_t *frame = decode_frame(frame_buf, ret);
        if(frame){
            if(frame->payload){
                free(frame->payload);
                frame->payload = NULL;
            }
            free(frame);
            frame = NULL;
        }
		printf("send[%d] ret=%d\n", i, ret);
	}
}

void send_beacon(void *para){
    printf("enter %s\n", __func__);
	uint8_t *host = HOST;
	struct sockaddr_in peer_addr_array[8];
	memset(peer_addr_array, 0, sizeof(struct sockaddr_in));
	uint32_t i = 0;
	for(i = 0; i < 8; i++){
		peer_addr_array[i].sin_family = AF_INET;
		inet_pton(AF_INET, host, &peer_addr_array[i].sin_addr);
		peer_addr_array[i].sin_port = htons(PEER_PORT+i);
	}
	uint8_t *buf = "i am beacon";
	uint8_t frame_buf[64] = {0};
	uint32_t buf_len = gen_frame(frame_buf, BEACON_TYPE, 0 , 0, 0, 0, strlen(buf),  buf);
	for(i = 0; i < 8; i++){
		uint32_t ret = sendto(sockfd, frame_buf, buf_len, 0, (struct sockaddr*)&peer_addr_array[i], sizeof(struct sockaddr));
		printf("send[%d] ret=%d\n", i, ret);
	}
}




uint32_t check_beacon_rsp(uint8_t *payload, uint32_t len){
	uint8_t beacon_rsp_data[] = {0XAA};
	if(memcmp(beacon_rsp_data, payload, len) == 0){
		return 1;
	}else{
		return 0;
	}
}

uint32_t check_timesolt_vaild(){
	if(time(NULL)%10 < 9){
		return 1;
	}else{
		return 0;
	}
}

uint8_t find_vaild_ip(){
	uint32_t i = 0;
	for(i = 0; i < 8; i++){
		if(addr_table[i].vaild == 0){
            printf("%s vaild ip:%d\n", __func__, i);
			return i;
		}
	}
	return 0xff;
}


uint32_t check_timeslot_vaild(uint32_t recv_time){
	uint32_t ret = 0;
	if(INTERVAL - (recv_time%INTERVAL) > 2){
		ret = 1;
	}else{
		ret = 0;
	}
	return ret;
}



void cal_next_req_time(struct timeval cur_time, uint8_t addr){
    struct timeval start_time;
    memset(&start_time, 0, sizeof(struct timeval));

}


void send_req_handle(void *para){
    printf("enter %s\n", __func__);
    if(!para){
        return;
    }
    uint8_t addr = ((uint8_t *)para)[0];

    printf("\n\n\nsend addr:%d\n\n\n", addr);
    free(para);

	if(addr_table[addr].rsp_flag == 1){
        addr_table[addr].rsp_flag = 0;	
        uint8_t *data = "i am req";
        uint32_t data_len = strlen(data);
        send_req(addr, data, data_len);
   	}else{
        addr_table[addr].vaild = 0;
    }
    printf("leave %s\n", __func__);
}


void send_dhcp(){
    printf("%s\n", __func__);
    uint8_t frame_buf[64] = {0};
    uint8_t data[32] = {0};
    static uint8_t addr =  0;
    addr = find_vaild_ip();
    uint32_t buf_len = 0;
    if(addr < 8){
        uint8_t *host = HOST;
        struct sockaddr_in peer_addr_array[8];
        memset(peer_addr_array, 0, sizeof(struct sockaddr_in));
        uint32_t i = 0;
        for(i = 0; i < 8; i++){
            peer_addr_array[i].sin_family = AF_INET;
            inet_pton(AF_INET, host, &peer_addr_array[i].sin_addr);
            peer_addr_array[i].sin_port = htons(PEER_PORT+i);
        }


        sch_event_t *req_event = (sch_event_t *)malloc(sizeof(sch_event_t));
        memset(req_event, 0, sizeof(sch_event_t));
        uint8_t event_name[32] = {0};
        sprintf(event_name, "req_%d", addr);
     
        uint32_t remain_tick = cal_remain_tick(addr);
           
        uint8_t remain_tick_format = remain_tick/INTERVAL;
        remain_tick_format <<= 5;
        remain_tick_format |= remain_tick%INTERVAL;        
//        uint8_t next_req_time = cal_next_time(addr);
//        data[0] = next_req_time;
//        data[0] = remain_tick_format;
        uint32_t data_len = 0;
        buf_len = gen_frame(frame_buf, DHCP_TYPE, 0 , 0, addr, remain_tick_format, data_len,  NULL);
        for(i = 0; i < 8; i++){
            uint32_t ret = sendto(sockfd, frame_buf, buf_len, 0, (struct sockaddr*)&peer_addr_array[i], sizeof(struct sockaddr));
            frame_t *frame = decode_frame(frame_buf, ret);
            if(frame){
                if(frame->payload){
                    free(frame->payload);
                    frame->payload = NULL;
                }
                free(frame);
                frame = NULL;
            }
            printf("send[%d] ret=%d\n", i, ret);
        }
        addr_table[addr].vaild = 1;
        addr_table[addr].rsp_flag = 1;

        uint8_t *arg = (uint8_t *)malloc(1);
        arg[0] = addr;

        initSchEvent(req_event, event_name, 0, 0, remain_tick, send_req_handle, (void *)arg);
        regSchEvent(scheduler, req_event);
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
	uint8_t buf[MAXLINE] = {0};
	uint32_t n = recvfrom(sockfd, buf, MAXLINE, 0, (struct sockaddr*)&peer_addr, &addrlen);	
	printf("receive len:%d\n", n);
	frame_t *frame = decode_frame(buf, n);
	if(frame){
		handle_frame(frame);	
	}
}


void init_addr_table(){
	uint32_t i = 0;
	for(i = 0; i < 8; i++){
		addr_table[i].vaild = 0;
		addr_table[i].addr = 0xff;
		addr_table[i].rsp_flag = 0;
	}
}




int32_t main(){

	uint32_t timer = 0;
	uint32_t interval = INTERVAL;
//	uint32_t app_tick = T;
	start_udp_server();
	if(sockfd < 0){
		return -1;
	}
	init_addr_table();
    scheduler = (scheduler_t *)malloc(sizeof(scheduler_t));
    memset(scheduler, 0, sizeof(scheduler_t));
    initScheduler(scheduler);
    startScheduler(scheduler);
    sch_event_t *def_event = (sch_event_t *)malloc(sizeof(sch_event_t));
    memset(def_event, 0, sizeof(sch_event_t));
    uint8_t name[32] = {0};
    strcpy(name, "def_event");
    initSchEvent(def_event, name, 1, 0, 0, send_beacon, NULL);
    regSchDefEvent(scheduler, def_event);
	while(1){
		struct timeval tv;
		fd_set readfds;
		uint32_t i=0;
		uint32_t n=0;
		FD_ZERO(&readfds);
		FD_SET(sockfd,&readfds);
		tv.tv_sec=0;
		tv.tv_usec=1000;
		select(sockfd+1,&readfds,NULL,NULL,&tv);
		if(FD_ISSET(sockfd,&readfds))
		{
			receive();
		}
//		app_tick++;
	}
}
