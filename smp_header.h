#include <stdint.h>
#define ETH_SMP_TYPE 0x88B5
#define ETH_SDS_TYPE 0x88B6

//specifies the role a communication endpoint is likely to play
enum role {
CLIENT = 0,
PEER = 1,
SERVER = 2,
TRACKER = 3,
PROXY = 4
};


//each SMP end point is identified by an application level−unique serviceID
//and the role played by the end point
struct smp_ep {
enum role r;
char serviceID[10];
};


//each SMP end point is temporarily mapped to a socket−like combination of
//<ETH address, port>
struct smp_ep_eth {
unsigned char eth_addr[6];
uint32_t port;
};
