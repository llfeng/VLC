
typedef struct{
	char status;
	char addr;
	int sockfd;
	int next_req_time;
}tag_t;


typedef enum{
	SLEEP = 0,
	ACTIVE,
	DHCP,
	LISTEN
}tag_status_t;
