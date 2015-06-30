#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <sys/socket.h>
#include <unistd.h>

/**** EP-Port Mapper Details ***/
#define SERVER_ADDR     "127.0.0.1"     /* localhost */
#define EPPM_PORT_NO	12345

#define EPPM_SENDBUFF_SIZE	2048

#define MAXBUF	1024



#include "smp_header.h"
#include "smp_pdu.h"





/******************************************** Application Level API ****************************************************/

/************************************************************************************************
A note about EPPM TOSs

TOS=1 => checking with EPPM about duplicate ServiceIDs and Roles
TOS=2 => checking with EPPM about duplicate port numbers used by non-server SMP endpoints
TOS=3 => checking with EPPM about source port number while sending SMP packet
TOS=4 => checking with EPPM about cached in SDS query results
TOS=5 => storing SDS query results in EPPM cache
TOS=6 => clearing appropriate entry in EPPM cache while closing a socket


*************************************************************************************************/


/****************************************************************************************************

	Utility function to connect to eppm local server by creating a Unix socket
Return value: socket file descriptor of the created socket

*****************************************************************************************************/
int eppm_interface()
{

	int sockfd;
    	struct sockaddr_in dest;
	char buffer[MAXBUF];

    	/*---- Create the socket. The three arguments are: ----*/
	/* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
    	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    	{
        	perror("Socket-eppm");
        	exit(errno);
    	}

   	/*---- Configure settings of the server address struct ----*/
    
	/* Address family = Internet */
	dest.sin_family = AF_INET;

	/* Set port number, using htons function to use proper byte order */
	dest.sin_port = htons(EPPM_PORT_NO);
    	if ( inet_aton(SERVER_ADDR, &dest.sin_addr.s_addr) == 0 )
	{
        	perror("SERVER_ADDR");
	        exit(errno);
	}

	/*---Connect to server---*/
	if ( connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0 )
	{
        	perror("Connect-eppm");
	        exit(errno);
	}else{
		printf("Connected to ep port mapper\n");
	}
	
	return sockfd;
}



/****************************************************************************************************
	Utility function to convert "aa:bb:cc:dd:ee:ff" form of hex notation of mac address
	given in argument1 'token' to 6 byte string which would be stored in argument2 'mac'
*****************************************************************************************************/
void format_hwaddr(char *token, char *mac){
	unsigned int iMac[6];
	int i;

	sscanf(token, "%x:%x:%x:%x:%x:%x", &iMac[0], &iMac[1],&iMac[2], &iMac[3], &iMac[4], &iMac[5]);
	for(i=0;i<6;i++)
    		mac[i] = (unsigned char)iMac[i];

}

/**************************************************************************************************
	Provides resolution of smp_ep structure{argument1} to smp_ep_eth structure{argument3}
	addrlen{argument2} and ethlen{argument4} are lengths of 
	smp_ep structure and smp_ep_eth structure respectively.

	Return value: -1 denotes failure in resolution and 0 means success so argument3 is properly set
***************************************************************************************************/
int directory_smp(const struct smp_ep *ep, socklen_t addrlen, struct smp_ep_eth *cli_smp_ep_eth, socklen_t ethlen)
{

	char buffer[1024];
	int r;
	char *token;


	/*For now smp_directory_service txt file is proxy for SDS Server*/
	FILE * fpsds = fopen("smp_directory_service","r");
	
	int flag=0;
	char mac_addr[6];


	//Search file for entry matching with ep structure provided
	while(!feof(fpsds))
	{
		fscanf(fpsds,"%s",buffer);
		token = strtok(buffer,",");	//tokenizing the string. Here token = serviceID
		if(strcmp(token,ep->serviceID)==0)	//matching serviceID
		{
			//now compare role
			token = strtok(NULL,",");
			r = atoi(token);
			if(r==ep->r)		//matching role
			{
				token = strtok(NULL,",");
				format_hwaddr(token,mac_addr);	//convert hex notation to character string

				/*Set the smp_ep_eth structure*/
				memcpy(cli_smp_ep_eth->eth_addr,mac_addr,6);
				cli_smp_ep_eth->port = (uint32_t)atoi(strtok(NULL,","));
				flag = 1;
				break;
			}
		}
	}
		
	fclose(fpsds);
	
	if(flag==1)
		return 0;	//resolution successfull
	else 
		return -1;	//resolution failed
}

/*********************************************************************************************
	Creates socket.
	1.Application provides a smp_ep struct ep{argument1} which signifies a unique socket endpoint
	2.Check for duplicates i.e. other applications running on the same machine having same servieID
	and role.
	3.If duplicate found return -2 else goto step4
	4.Generate a random 32 bit port no. and register with port mapper using that port number,
	hw address,serviceID and role. If there is a port number conflict than generate another random
	port number and repeat till successfull.
	5.Create raw socket
	6.Find interface index and mac address to bind to
	
Return value: returns socket file descriptor on success and negative number on failure.


********************************************************************************************/

int socket_smp(const struct smp_ep *ep, socklen_t addrlen)
{
	
	
	int flag=0;
	int smpfd;

	//connect to local eppm server
	int eppm_sockfd = eppm_interface();
	char eppmSendBuff[EPPM_SENDBUFF_SIZE];
	//TOS = 1
	sprintf(eppmSendBuff,"%d%s%d",1,ep->serviceID,ep->r);
	//send query
	send(eppm_sockfd,eppmSendBuff,sizeof(eppmSendBuff),0);
	//receive query
	recv(eppm_sockfd,&flag,sizeof(int),0);
/*	printf("flag recieved:%d\n",flag);*/
	//closing eppm_sockfd
	close(eppm_sockfd);


	if(flag==-1)
		return -2;
	else
	{

		smpfd = socket(AF_PACKET,SOCK_RAW,htons(ETH_SMP_TYPE));
		
		if (smpfd < 0) {
				perror("Socket creation failed!!");
	    		return -1;
		}
		
		/*Set network interface index and store in ifindex*/
		struct ifreq if_idx;	//find the structure definition in /usr/include/net/if.h file
		memset(&if_idx, 0, sizeof(struct ifreq));
		strncpy(if_idx.ifr_name, "eth0", IFNAMSIZ-1);
		//didn't fully understand purpose of this snippet
		if (ioctl(smpfd,SIOCGIFINDEX,&if_idx)==-1) {
	    	perror("Interface index not set");
		}
		int ifindex = if_idx.ifr_ifindex;		//defining interface index
		//printf("index:%d\n",ifindex);
		
		/*Get MAC Address using if_mac structure*/
		struct ifreq if_mac;
		memset(&if_mac, 0, sizeof(struct ifreq));
		strncpy(if_mac.ifr_name, "eth0", IFNAMSIZ-1);
		if (ioctl(smpfd, SIOCGIFHWADDR, &if_mac) < 0){
/*			printf("Request to get mac failed!!\n");*/
    		perror("SIOCGIFHWADDR");
		}

		
		//removed const - CHECK AGAIN
		unsigned char ether_serv_haddr[6];
		memcpy(ether_serv_haddr,if_mac.ifr_hwaddr.sa_data,6); //client ethernet address

		//printf("%02x %02x %02x\n", ether_serv_haddr[0],ether_serv_haddr[1],ether_serv_haddr[2]);

		//structure definition found in /usr/include/netpacket/packet.h file
		/*Set the sockaddr_ll structure */	
		struct sockaddr_ll addr = {0};  //Clear all bits first
		addr.sll_family = AF_PACKET;
		addr.sll_ifindex = ifindex;     //Interface index calculated previously
		addr.sll_halen = ETHER_ADDR_LEN;	//ETHER_ADDR_LEN found in /usr/include/net/ethernet.h
		addr.sll_protocol = htons(ETH_SMP_TYPE); //
		memcpy(addr.sll_addr, ether_serv_haddr, ETHER_ADDR_LEN);
				
		//bind the socket
		if(bind(smpfd,(struct sockaddr *)&addr,sizeof(addr))<0){
			perror("bind");
		}
		
		if((ep->r) != 2)	//non server end point
/*		if(1)*/
		{	
			/*You gotta register the client first with EP-Port Mapper*/
			//interface to eppm
/*			printf("Connecting to pm...\n");*/
			int eppm_sockfd = eppm_interface();
			uint32_t src_port = (uint32_t)rand();
			int brc = -1;
			while(brc==-1)
			{
				src_port = (uint32_t)rand();
				//clearing eppmSendBuff
				bzero(&eppmSendBuff, sizeof(eppmSendBuff));
				
				//TOS = 2
				printf("src port: %d\n",src_port);
				sprintf(eppmSendBuff,"%d%d%s%d%s%u",2,smpfd,ep->serviceID,ep->r,ether_serv_haddr,src_port);
				if((send(eppm_sockfd,eppmSendBuff,sizeof(eppmSendBuff),0)) < 0){
					perror("Send");

				}else{
/*					printf("Request to register port with ep-port mapper sent\n");*/
				}
				if(recv(eppm_sockfd,&brc,sizeof(int),0) < 0){
					perror("recv response");
				}else{
/*					printf("Successfully recieved ep-port mapper response\n");*/
				}
				//if brc=1 then loop will be broken in the next iteration

			}
				
			//close eppm_sockfd
			close(eppm_sockfd);
			
		}
		else 	//server end point
		{
			
/*			printf("Connecting to pm...\n");*/
			int eppm_sockfd = eppm_interface();
			uint32_t src_port = (uint32_t)rand();
			int brc = -1;
/*			while(brc==-1)*/
/*			{*/
/*				src_port = (uint32_t)rand();*/
				//contact directory service to get src_port number
				struct smp_ep_eth *ser_smp_ep_eth = malloc(sizeof(struct smp_ep_eth));
				directory_smp(ep, addrlen, ser_smp_ep_eth, sizeof(struct smp_ep_eth));
				
				//fill in sec_port for server
				src_port = (uint32_t)ser_smp_ep_eth->port;
				
				//clearing eppmSendBuff
				bzero(&eppmSendBuff, sizeof(eppmSendBuff));
				
				//TOS = 2
/*				printf("src port: %d\n",src_port);*/
				sprintf(eppmSendBuff,"%d%d%s%d%s%u",2,smpfd,ep->serviceID,ep->r,ether_serv_haddr,src_port);
				if((send(eppm_sockfd,eppmSendBuff,sizeof(eppmSendBuff),0)) < 0){
					perror("Send");

				}else{
					printf("Request to register port with ep-port mapper sent\n");
				}
				if(recv(eppm_sockfd,&brc,sizeof(int),0) < 0){
					perror("recv response");
				}else{
/*					printf("Successfully recieved ep-port mapper response\n");*/
				}
				//if brc=1 then loop will be broken in the next iteration

/*			}*/
				
			//close eppm_sockfd
			close(eppm_sockfd);
		}
	
	}

	return smpfd;

}
/**Closes socket and terminates connection**/
int close_smp(int smpfd)
{
	//send smpfd to eppm
	int eppm_sockfd = eppm_interface();
	
	int sendBuff[2] = {6,smpfd};
	
	
	//TOS = 6
	send(eppm_sockfd,sendBuff, sizeof(sendBuff),0);
	
	
	//close eppm_sockfd
	close(eppm_sockfd);
	
	return close(smpfd);
}


//checked - compiled correctly
int sendto_smp(int smpfd,const void *buf,size_t bytes, const struct smp_ep *to, socklen_t addrlen)
{


	struct smp_ep_eth *cli_smp_ep_eth = (struct smp_ep_eth*)malloc(sizeof(struct smp_ep_eth));
	
	
	//first check with eppm cache
	
	//connecting..
	int eppm_sockfd = eppm_interface();
	
	char eppm_sendBuff[EPPM_SENDBUFF_SIZE];
	
	
	//TOS = 4 => Retrieving hw address and port from service id and role from cache
	sprintf(eppm_sendBuff,"%d%s%d",4,to->serviceID,to->r);
	
	//testing
/*	printf("Printing eppm_sendbuff: %s\n",eppm_sendBuff);*/
	
	send(eppm_sockfd, eppm_sendBuff, sizeof(eppm_sendBuff),0);
	
	//test
/*	printf("Sent to eppm successfully\n");*/
	
	char eppm_recvBuff[EPPM_SENDBUFF_SIZE];
	
	recv(eppm_sockfd,eppm_recvBuff, sizeof(struct smp_ep_eth),0);
	
	//testing
/*	printf("Received from eppm in TOS=4: %s\n",eppm_recvBuff);*/
	

	//closing eppm_sockfd
	close(eppm_sockfd);


	if(eppm_recvBuff[0]!='\0')
	{
		cli_smp_ep_eth = (struct smp_ep_eth*)eppm_recvBuff;
	}
	else
	{	
		if(directory_smp(to, addrlen, cli_smp_ep_eth, sizeof(struct smp_ep_eth)) == 0)
		{
			printf("Destination Resolved successfully!!\n");
			printf("%d\n",cli_smp_ep_eth->port);
			printf("%s \n",cli_smp_ep_eth->eth_addr);
			bzero(&eppm_sendBuff, sizeof(eppm_sendBuff));
			

			//open interface to eppm
			int eppm_sockfd = eppm_interface();

			//TOS = 5 => storing in local cache
			sprintf(eppm_sendBuff,"%d%s%d%s%u",5,to->serviceID,to->r, cli_smp_ep_eth->eth_addr,cli_smp_ep_eth->port);

			//test
/*			printf("Testing for TOS = 5: %s\n",eppm_sendBuff);*/
			
			//cache the directory service result in ep port mapper process
			send(eppm_sockfd, eppm_sendBuff, sizeof(eppm_sendBuff),0);
			

			//closing eppm_sockfd
			close(eppm_sockfd);

		}else{
			printf("Resolution Failed\n");
		}
	}
	
	
	//removed const - CHECK AGAIN
	unsigned char ether_serv_haddr[6];
	//ethernet address of destination
	memcpy(ether_serv_haddr,cli_smp_ep_eth->eth_addr,6); 



	
	
	//get the index of the interface of sender
	struct ifreq if_idx;
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, "eth0", IFNAMSIZ-1);
	if (ioctl(smpfd, SIOCGIFINDEX, &if_idx) < 0)
	    perror("SIOCGIFINDEX");
	
	
	//get the MAC address of sender
	struct ifreq if_mac;
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, "eth0", IFNAMSIZ-1);
	if (ioctl(smpfd, SIOCGIFHWADDR, &if_mac) < 0)
	    perror("SIOCGIFHWADDR");
	
	
	//constructing ethernet header
	int tx_len = 0;
	char sendbuf[2048];	//actual SMP pdu is of 1024B payload + 12B smp header
	struct ether_header *eh = (struct ether_header *) sendbuf;
	memset(sendbuf, 0, 2048);
	/* Ethernet header */
	eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
	eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
	eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
	eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
	eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
	eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
	eh->ether_dhost[0] = ether_serv_haddr[0];
	eh->ether_dhost[1] = ether_serv_haddr[1];
	eh->ether_dhost[2] = ether_serv_haddr[2];
	eh->ether_dhost[3] = ether_serv_haddr[3];
	eh->ether_dhost[4] = ether_serv_haddr[4];
	eh->ether_dhost[5] = ether_serv_haddr[5];
	eh->ether_type = htons(ETH_SMP_TYPE);
	tx_len += sizeof(struct ether_header);
	
/*	printf("Printing partial sendbuf: ethernet hdr:%s\n", sendbuf);*/
	


	//constructing SMP PDU
	struct smp_pdu *cli_pdu = (struct smp_pdu*)malloc(sizeof(struct smp_pdu));
	
	//set version and profile
	cli_pdu->version = 0;
	cli_pdu->profile = 0;
	//set payload length in bytes
	cli_pdu->payload_length = bytes;
	cli_pdu->dest_port = cli_smp_ep_eth->port;

	
	cli_pdu->payload_length = 1024;	//CHECK AGAIN
	cli_pdu->dest_port = cli_smp_ep_eth->port;
	
	//get source port number from ep port mapper
	//int eppm_buff[2] = {3,smpfd};
	char eppm_buff[10];
	sprintf(eppm_buff,"%d%d",3,smpfd);
	//interface to eppm
	eppm_sockfd = eppm_interface();

	//TOS = 3
	//testing
//	printf("Sending to eppm: %s\n",eppm_buff);
	send(eppm_sockfd, eppm_buff, 2,0);
	
	recv(eppm_sockfd, &(cli_pdu->src_port), sizeof(uint32_t),0);
	

	//closing eppm_sockfd
	close(eppm_sockfd);


	memcpy(cli_pdu->payload,buf,cli_pdu->payload_length);
	
	//printf("App Buf: %s and cli_pdu payload: %s\n", buf, cli_pdu->payload);
	//fill in SMP pdu as a payload to ethernet
	/* Packet data */
	memcpy(sendbuf+tx_len,cli_pdu,sizeof(struct smp_pdu));
	tx_len++;
/*	printf("Complete ETHERNET Frame: %s\n", sendbuf);*/
/*	printf("Cli_pdu payload: %s\n", ((struct smp_pdu *)(sendbuf+tx_len-1))->payload);*/
	//send the raw ethernet frame
	struct sockaddr_ll socket_address;
	/* Index of the network device */
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/*Family Type*/
	socket_address.sll_family = AF_PACKET;
	/*Protocol type*/
	socket_address.sll_protocol = htons(ETH_SMP_TYPE);
	/* Destination MAC */
	socket_address.sll_addr[0] = ether_serv_haddr[0];
	socket_address.sll_addr[1] = ether_serv_haddr[1];
	socket_address.sll_addr[2] = ether_serv_haddr[2];
	socket_address.sll_addr[3] = ether_serv_haddr[3];
	socket_address.sll_addr[4] = ether_serv_haddr[4];
	socket_address.sll_addr[5] = ether_serv_haddr[5];
	/* Send packet */
	int numBytes = sendto(smpfd, sendbuf, tx_len+sizeof(struct smp_pdu), 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll));
	
	if(numBytes<0){
		perror("sendto failed");
	}else{
		printf("Successs sendto\n\n");
	}
	
	//close eppm_sockfd
	close(eppm_sockfd);

	return numBytes;
}


void gen_rdm_bytestream(unsigned char *stream , size_t num_bytes)
{

  size_t i;
  time_t t;
  srand((unsigned)time(&t)); //generate seed for rand()
  for (i = 0; i < num_bytes; i++)
  {
    stream[i] = rand();
  }

  return;
}

//checked - compiled correctly
int recvfrom_smp(int smpfd,void *buf,size_t bytes, struct smp_ep *from,socklen_t *addrlen)
{	

	/*recv_buffer will hold the entire ethernet frame*/
	char recv_buffer[2048];

	/*payload pointer will point to smp_pdu*/
	int eth_header_size = sizeof(struct ether_header);
	char *payload_ptr = recv_buffer + eth_header_size;

	/*Store Extracted smp_pdu in this structure*/
	struct smp_pdu *recvd_pdu = (struct smp_pdu*)malloc(sizeof(struct smp_pdu));

	int numbytes = -1;
	uint32_t my_port;
/*	printf("Waiting to recieve!!\n");*/
	while(numbytes < 0)
	{
/*		printf("About to recieve\n");*/
		numbytes = recvfrom(smpfd,recv_buffer,2048,0,NULL,NULL);
		printf("recvd_pdu\n");
		if(numbytes < 0){
			perror("Error in recieving!!");
		}else{
/*			printf("Recieved something\n");*/

			//test
/*			printf("%s\n",recv_buffer);*/

			//interface to eppm
			int eppm_sockfd = eppm_interface();

			char eppm_buff[50];
			//setting
			bzero(&eppm_buff,50);
			sprintf(eppm_buff,"%d%d",3,smpfd);
/*			eppm_buff[2] = '\0';*/
			//TOS = 3
			send(eppm_sockfd, eppm_buff, 2,0);
	
			recv(eppm_sockfd, &(my_port), sizeof(uint32_t),0);

			//closing eppm_sockfd
			close(eppm_sockfd);

			//obtaining smp_pdu from ethernet frame

			memcpy(recvd_pdu,payload_ptr,sizeof(struct smp_pdu));
			//test
			printf("payload lalala:%s\n", recvd_pdu->payload);

			//Multiplexing
			 if(recvd_pdu->dest_port == my_port)
			 {
			 	break;
			 }
			 else
			 {
			 	//test
			 	printf("Dest port no. from recvd pdu = %d\n",recvd_pdu->dest_port);
			 	printf("My port = %d\n",my_port);
			 	
			 	
				printf("Not ours\n");
				numbytes = -1;
			 }

		}
	}
	
	
/*	printf("ETHERNET FRAME:%s\n",recv_buffer);*/
/*	printf("SMP PDU:%s\n",payload_ptr);*/


	//printf("Recieved payload: %s\n",recvd_pdu->payload);
	
	//generating smp_ep_eth structure for the client
	struct smp_ep_eth *cli_smp_ep_eth = (struct smp_ep_eth*)malloc(sizeof(struct smp_ep_eth));
	
	//copying contents of smp payload to buf
	memcpy((char*)buf,recvd_pdu->payload,1024);
/*	printf("buf:%s\n",(char*)buf);*/


	if(from->r!=2)	//a server smp-endpoint send
	{
		//obtaining client port number
		cli_smp_ep_eth->port = recvd_pdu->src_port;
		
		//extracting ethernet header to get source ethernet address
		char recv_eth_header[eth_header_size];
		memcpy(recv_eth_header,recv_buffer,eth_header_size);
		
		struct ether_header *eh = (struct ether_header *)recv_eth_header;
		
		cli_smp_ep_eth->eth_addr[0]= eh->ether_shost[0];
		cli_smp_ep_eth->eth_addr[1]= eh->ether_shost[1];
		cli_smp_ep_eth->eth_addr[2]= eh->ether_shost[2];
		cli_smp_ep_eth->eth_addr[3]= eh->ether_shost[3];
		cli_smp_ep_eth->eth_addr[4]= eh->ether_shost[4];
		cli_smp_ep_eth->eth_addr[5]= eh->ether_shost[5];
		
		
		//so we have cli_smp_ep_eth ready. We also have smp_ep *from. Register both of them in EP Port Mapper service.
		//right now just right it in the file
		
		//now create a temp smp_ep structure
		struct smp_ep *cli_smp_ep = (struct smp_ep*)malloc(sizeof(struct smp_ep));
		
		//Need to generate it between zzzzzaaaaa to zzzzzzzzzz 
		char tempSID[10] = "zzzzzzzzzz";
		int i;
		for(i = 5; i < 10; i++){
			char rndByte[1];
			gen_rdm_bytestream(rndByte,1);
			tempSID[i] = (97 + (((unsigned int)rndByte[0])%26) ); 
			//wait(100);
		}

		printf("%s\n", tempSID);

		memcpy(cli_smp_ep->serviceID,tempSID,10);
		cli_smp_ep->r = 0;
		
		//storing it in "from" provided by application
		//removed const qualifier in the function argument - CHECK AGAIN
		memcpy(from,cli_smp_ep,sizeof(struct smp_ep));

		//register in directory service
	/*	FILE *fp = fopen("smp_directory_service","a");*/
	/*	//ignoring authoritative for now - CHECK AGAIN*/
	/*	fprintf(fp,"\n%s,%d,",cli_smp_ep->serviceID,cli_smp_ep->r);*/
	/*	fprintf(fp,"%x:%x:%x:%x:%x:%x,",*(cli_smp_ep_eth->eth_addr),*((cli_smp_ep_eth->eth_addr)+1),*((cli_smp_ep_eth->eth_addr)+2),*((cli_smp_ep_eth->eth_addr)+3),*((cli_smp_ep_eth->eth_addr)+4),*((cli_smp_ep_eth->eth_addr)+5));*/
	/*	fprintf(fp,"%d,%d", cli_smp_ep_eth->port,3600);*/

	/*	//printf("%02x %02x %02x\n", ether_serv_haddr[0],ether_serv_haddr[1],ether_serv_haddr[2]);*/
	/*	fclose(fp);*/
	/*	printf("Done recieving!!\n");*/

		//register smp_ep with eppm
		int eppm_sockfd = eppm_interface();
		
		char eppm_sendBuff[EPPM_SENDBUFF_SIZE];
		
		bzero(&eppm_sendBuff, sizeof(eppm_sendBuff));
				
		//TOS = 5 => storing in local cache
		// sprintf(eppm_sendBuff,"%d%s%s",5,(char*)cli_smp_ep, (char*)cli_smp_ep_eth);
		sprintf(eppm_sendBuff,"%d%s%d%s%u",5,cli_smp_ep->serviceID,cli_smp_ep->r,cli_smp_ep_eth->eth_addr,cli_smp_ep_eth->port);

		//cache smp_ep structure in eppm cache
		send(eppm_sockfd, eppm_sendBuff, sizeof(eppm_sendBuff),0);

		//closing eppm_sockfd
		close(eppm_sockfd);
	}
	return numbytes;

}
