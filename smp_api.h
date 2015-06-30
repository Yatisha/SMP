#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>


#include "smp_header.h"





/******************************************** Application Level API ****************************************************/


int socket_smp(const struct smp_ep *ep, socklen_t addrlen);
int sendto_smp(int smpfd,const void *buf,size_t bytes, const struct smp_ep *to, socklen_t addrlen);
int recvfrom_smp(int smpfd,void *buf,size_t bytes, const struct smp_ep *from,socklen_t *addrlen);
int directory_smp(const struct smp_ep *ep, socklen_t addrlen, const struct smp_ep_eth *cli_smp_ep_eth, socklen_t ethlen);
