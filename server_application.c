#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "smp_header.h"

int main()
{

		//creating buffer space for receiving data
		char recvData[1024];
	
		//emptying the space
		bzero(recvData,sizeof(recvData));

		/*Make structures for client and server*/
		struct smp_ep *cli_ep = (struct smp_ep *)malloc(sizeof(struct smp_ep *));
		struct smp_ep *server_ep = (struct smp_ep *)malloc(sizeof(struct smp_ep *));	
	
		/*Set server structure to bind to socket*/
		memcpy(server_ep->serviceID , "weruowrwds",10);
		server_ep->r = 2;
	
		//create smp socket
		int smpfd = socket_smp(server_ep, sizeof(struct smp_ep));
		if(smpfd == -2){
			printf("Service id Already exists!\n");
		}else if(smpfd == -1){
			printf("Socket creation failed\n");
		}else{
			printf("Socket created successfully\n");
		}
	
		int addrlen = sizeof(struct smp_ep);
	while(1)
	{
		int bytesRecvd;
		//receive data from client
		printf("Ready to recieve from client\n");
		bytesRecvd = recvfrom_smp(smpfd,recvData,sizeof(recvData),cli_ep,&addrlen);

		if(bytesRecvd==-1)
		{
			printf("Error in receiving data\n");
			return 0;

		}else{

			printf("Application data successfully recieved\n");
			//test if recv_buffer contains the data
			printf("Application data recieved: %s\n",recvData);
		}
	
	
	
	
		//send the same data back again
		int byteSent;
		//send the data
		printf("Echoing back to client\n");
		byteSent = sendto_smp(smpfd,recvData,sizeof(recvData), cli_ep, sizeof(struct smp_ep));
	
		if(byteSent==-1)
		{
			printf("Error while sending\n");
			return 0;
		}
	

	}

	return 0;
}
