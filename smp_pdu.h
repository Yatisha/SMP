#include <stdint.h>
struct smp_pdu{

	uint8_t version;
	uint8_t profile;
	uint16_t payload_length;
	uint32_t dest_port;
	uint32_t src_port;
	char payload[1024];
	
};


struct sds_request_pdu{

	uint8_t version;
	uint8_t type;
	uint16_t request_length;
	uint32_t transaction_ID;
	char service_ID[10];
	uint16_t role;

};


struct sds_response_pdu{

	uint8_t version;
	uint8_t type;
	uint16_t response_length;
	uint32_t transaction_ID;
	char service_ID[10];
	uint16_t role;
	char eth_addr[6];
	uint16_t ttl;
	uint32_t port;
	
	

};

