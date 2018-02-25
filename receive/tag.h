
typedef struct{
	char status;
	char addr;
	int sockfd;
	struct timeval next_req_time;
}tag_t;


typedef enum{
	SLEEP = 0,
	ACTIVE,
	DHCP,
	LISTEN
}tag_status_t;
