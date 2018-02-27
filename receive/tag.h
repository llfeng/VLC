
typedef struct{
	uint8_t status;
	uint8_t  addr;
	uint32_t sockfd;
    uint32_t next_req_time;
}tag_t;


typedef enum{
	SLEEP = 0,
	ACTIVE,
	DHCP,
	LISTEN
}tag_status_t;
