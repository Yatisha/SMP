#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "smp_api.h"

int main()
{

	char sendData[512];
	
	int smpfd;
	struct smp_ep *cli_ep = (struct smp_ep *)malloc(sizeof(struct smp_ep));
	char cli_service_id[10] = "client2341";
	memcpy(cli_ep->serviceID , cli_service_id,10);
	cli_ep->r = 0;

	struct smp_ep *server_ep = (struct smp_ep *)malloc(sizeof(struct smp_ep));
	char ser_service_id[10] = "weruowrwds";
	memcpy(server_ep->serviceID ,ser_service_id ,10);
	server_ep->r = 2;
	
	//create smp socket
	smpfd = socket_smp(cli_ep, sizeof(struct smp_ep));
	if(smpfd > 0)
		printf("Socket successfully created!!\n");
	else if(smpfd == -2){
		printf("Duplicate serviceID!!\n");
		exit(1);
	}else{
		printf("Socket creation failed!!\n");
		exit(1);
	}

    while(1){
    
    
        printf("\n\nEnter data to be sent to server>");
        fgets(sendData,1024,stdin);
	    int byteSent;
	    //send the data
	    byteSent = sendto_smp(smpfd,sendData,sizeof(sendData), server_ep, sizeof(struct smp_ep));
	
    	if(byteSent==-1)
    	{
    		printf("Error while sending\n");
    		return 0;
    	}
    	
    	char recv_buffer[1024];
	
	
    	//emptying recv_buffer
    	bzero(recv_buffer,sizeof(recv_buffer));
    	
    	int addrlen = sizeof(struct smp_ep);

    	//receive data from server
    	int byteRecvd = recvfrom_smp(smpfd,recv_buffer,sizeof(recv_buffer),server_ep,&addrlen);
    	printf("Recieving from server.....\n");
    	if(byteRecvd==-1)
    	{
    		printf("Error in receiving data\n");
    		return 0;
    	}
	
    	//test if recv_buffer contains the data
    	printf("Data echoed back from server: %s",recv_buffer);
    
    }

	return 0;
}
