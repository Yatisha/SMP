/**************************************************************************************************************
	EP-Port Mapper
	This process will run independently of all applications.
	Applications communicate with it by sending and recieving through unix local sockets
	An application will send a TOS followed by various parameters.
	TOS determines the type of functionality the application is requesting
****************************************************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "smp_header.h"
#define MY_PORT		12345
#define MAXBUF		2048

#define FILEBUFF_SIZE	100

int main()
{   
	int sockfd;
	struct sockaddr_in addr;

	//clearing eppm_cache file
	FILE *fp;
	fp = fopen("eppm_cache","w");
	fclose(fp);
	
	



 	/*---- Create the socket. The three arguments are: ----*/
	/* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		perror("Socket");
		exit(errno);
	}

	/*---- Configure settings of the server address struct ----*/
	
	/* Address family = Internet */
	addr.sin_family = AF_INET;
	/* Set port number, using htons function to use proper byte order */
	addr.sin_port = htons(MY_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	/*---- Bind the address struct to the socket ----*/
	if ( bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
	{
		perror("socket--bind");
		exit(errno);
	}

	/*---- Listen on the socket, with 20 max connection requests queued ----*/
	if ( listen(sockfd, 20) != 0 )
	{
		perror("socket--listen");
		exit(errno);
	}
	else
	{
		printf("Listening\n");
	}
	
	 
	int TOS=0;
	
	char fileBuff[FILEBUFF_SIZE];
	char buff[MAXBUF];
    char * buffer;
	char *token;
	while(1)
	{
		int clientfd;
		buffer = &buff[0];
		/*---accept a connection (creating a data pipe)---*/
		printf("\n\nReady to accept connection\n");
		clientfd = accept(sockfd,NULL,NULL);
		if(clientfd == -1){
			perror("Eppm-accept");
		}else{
			printf("Accepted connection\n");
		}
	
		//clearing buffer
		bzero(&buff, sizeof(buff));
		//recv data in buffer
		int n = recv(clientfd, buff, sizeof(buff),0);
		if(n > 0){
			printf("Successfully Recieved!!\n");
		}else{
			perror("Recieving Request");
		}
		printf("Buffer recieved: %s\n",buff);
		printf("Length of buffer = %d\n",(int)strlen(buffer));

		//cleaning job
		// buffer = buffer+(int)strlen(buffer);
		// bzero(buffer,sizeof(buff)-(int)strlen(buffer));


		char TOSbuff[2];

		TOSbuff[0] = *buff;
		TOSbuff[1] = '\0';

		//test
		printf("TOSbuff:%s",TOSbuff);
		// memcpy(&TOSbuff,buff,1);
		TOS = atoi(TOSbuff);
		printf("TOS:%d\n", TOS);

		/*Clear fileBuff for each run*/
		bzero(fileBuff,FILEBUFF_SIZE);

		switch(TOS)
		{
			case 1:	
			{
				buffer = buff + 1;
				char sID[11];
				memcpy(sID,buffer,10);
				sID[10] = '\0';
				printf("%s\n", sID);

				int role = (atoi(buffer+10));
				//check eppm_cache file for serviceID and role
				fp = fopen("eppm_cache","r");
				int flag = 1;
				//printf("feof:%d\n", feof(fp));
				//while(!feof(fp))
				while(1)
				{	
					//test
					// printf("reached here before fscanf\n");
					//check whether file is empty or not
					int n = fscanf(fp,"%s",fileBuff);
					// printf("fileBuff:%s\n", fileBuff);

					//test
					printf("scanned Successfully\n");

					//end of file reached
					if(n == EOF)
						break;

					printf("Filebuff - %s",fileBuff);
					token = strtok(fileBuff,",");	//tokenizing the string. Here token = smpfd
					if(atoi(token) == -1)
						continue;
					token = strtok(NULL,",");	//here token = serviceID
					if(strcmp(token,sID)==0)	//matching serviceID
					{
						flag = -1;
						break;
					}
				}
				//we need to add this sID and role to file but that will be taken care of in case 2
				//sending 1 or -1 appropriately
				printf("Port mapper sending flag %d\n",flag);
				int n = send(clientfd,&flag,sizeof(int),0);
				if(n > 0){
					printf("Port Mapper:Successfully sent\n");
				}else{
					perror("port-mapper-Send");
				}

				//close file eppm_cache
				close(fp);
				close(clientfd);
				break;
			}
			case 2:	//Receiving <smpfd,serviceID,role,ether_addr,port> <%d,%s(10),%d,%s(6),%u(32b)
			{	
				//test
				printf("Case 2:printing smpfd: %c\n",buff[1]);
				buffer = buff + 1;
				if(*buffer == '-')
					buffer++;
				int i=0;
				char port_buff[100];
				while(*(buffer+18+i)!=0)
				{
					//printf("here\n");
					port_buff[i] = *(buffer+18+i);
					i++;
				}


				uint32_t port = atoi(port_buff);
				//test
				printf("printing port no in case 2 %d\n",port);
				fp = fopen("eppm_cache","r");
				int flag = 1;
				while(1)
				{	
					int n = fscanf(fp,"%s",fileBuff);

					//end of file reached
					if(n == EOF)
						break;

					token = strtok(fileBuff,",");	//tokenizing the string. Here token = smpfd
					
					/*if -1 then invalid file descriptor so ignore*/
					if(atoi(token) == -1)
						continue;
					
					token = strtok(NULL,",");	//here token = serviceID
					token = strtok(NULL,",");	//here token = role
					token = strtok(NULL,",");	//here token = ether addr
					token = strtok(NULL,",");	//here token = port
					uint32_t port_from_cache = (uint32_t)(atoi(token));
					printf("%u",port_from_cache);
					if(port==port_from_cache)	//matching serviceID
					{
						flag = -1;
						break;
					}
				}
				send(clientfd,&flag,sizeof(int),0);
				
				if(flag==1)
				{
					//register in ep_portmapper
					fclose(fp);
					fp = fopen("eppm_cache","a");
					
					//not sure
					//test
					printf("printing smpfd: %c\n",*buffer);
					fprintf(fp,"%c,",*buffer);
					//test
					// printf("goint to write sid in case 2\n");
					buffer++;
					for(i=0;i<10;i++,buffer++){
						//test
						//printf("%c\n",*(buffer));
						fprintf(fp,"%c",*(buffer));
					}
					//printing role,eth_addr,port
					fprintf(fp,",%c,",*(buffer));
					
					buffer++;
					for(i=0;i<5;i++,buffer++)
					{
						fprintf(fp,"%c",*(buffer));
					}
					fprintf(fp,"%c,",*buffer);
					buffer++;

					while(*buffer!='\0')
					{
						fprintf(fp,"%c",*buffer);
						buffer++;
					}
					fprintf(fp,"\n");
					fclose(fp);
				}
                close(clientfd);
				break;
			}
			case 3://getting source port number from smpfd
			{
				fp = fopen("eppm_cache","r");
				buffer = buff + 1;
				// int smpfd = (atoi(buffer));
				char smpfdchar[2];
				smpfdchar[0]= *buffer;
				smpfdchar[1] = '\0';
				int smpfd = atoi(smpfdchar);
				//test
				printf("TOS3: smpfdchar:%s\n",smpfdchar);
				printf("TOS 3: smpfd:%d\n",smpfd);

				while(1)
				{	
					fscanf(fp,"%s",fileBuff);

					//end of file reached
					if(*fileBuff=='\0')
						break;

					int smpfd_cache = atoi(strtok(fileBuff,","));	//tokenizing the string. Here token = smpfd
					if(smpfd_cache == -1)
						continue;
					if(smpfd == smpfd_cache){
						token = strtok(NULL,",");	//here token = serviceID
						
						token = strtok(NULL,",");	//here token = role
						
						token = strtok(NULL,",");	//here token = ether addr
						
						token = strtok(NULL,",");	//here token = port

						uint32_t port_from_cache = (uint32_t)atoi(token);
						send(clientfd,&port_from_cache,sizeof(uint32_t),0);
						break;
					}
					
				}

				fclose(fp);
				close(clientfd);
				break;
			}
			case 4://TOS=4-Retrieving hw address and port from sid and role from cache,<%s%d>-serviceID,role in buffer
			{
				buffer = buff + 1;
				
				char sid[11];
				memcpy(sid,buffer,10);
				sid[10] = '\0';
				int r = atoi(buffer + 10);
				printf("%s %d\n", sid,r);

				struct smp_ep_eth * send_ep_eth = (struct smp_ep_eth *)malloc(sizeof(struct smp_ep_eth ));
				bzero(send_ep_eth,sizeof(struct smp_ep_eth));
				
				/*open cache file*/
				fp = fopen("eppm_cache","r");
				if(fp<0)
					perror("File open error");

				while(1)
				{	
					bzero(fileBuff,FILEBUFF_SIZE);
					fscanf(fp,"%s",fileBuff);
					printf("Done1\n");
					//end of file reached
					printf("fileBuff: %s\n", fileBuff);
					if((*(fileBuff))=='\0')
						break;
					printf("Done2\n");
					token = strtok(fileBuff,",");	//tokenizing the string. Here token = smpfd
					
/*					if(atoi(token) == -1)*/
/*						continue;*/

					char *sid_cache = strtok(NULL,",");	//here token = serviceID
					int r_cache = atoi(strtok(NULL,","));	//here token = role
					printf("sid_cache:%s r_cache: %d",sid_cache,r_cache);
					if(strcmp(sid_cache,sid) == 0 && r_cache == r)
					{	
						char * eth_haddr = strtok(NULL,",");	//here token = ether addr
						token = strtok(NULL,",");	//here token = port
						uint32_t port_from_cache = (uint32_t)atoi(token);
						//test
						printf("port from cache lalalalla: %d\n", port_from_cache);
						memcpy(send_ep_eth->eth_addr,eth_haddr,6);
						send_ep_eth->port = port_from_cache;
						break;
					
					}
				}

				if(send(clientfd,send_ep_eth,sizeof(struct smp_ep_eth),0) < 0){
					perror("Send error case4");
				}

				fclose(fp);
				close(clientfd);
				break;
			}
			case 5://TOS=5-storing in local cache<%s%s>smp_ep and smp_eth structure
			{
				buffer = buff + 1;
				struct smp_ep * ep= (struct smp_ep *)malloc(sizeof(struct smp_ep));
				struct smp_ep_eth * epth= (struct smp_ep_eth *)malloc(sizeof(struct smp_ep_eth));

				// memcpy(ep,buffer,sizeof(struct smp_ep));
				// memcpy(epth,buffer + sizeof(struct smp_ep) ,sizeof(struct smp_ep_eth));

				//transfering serviceID
				memcpy(ep->serviceID,buffer,10);
				buffer = buffer+10;

				//transfering role
				char epr;
				memcpy(&epr,buffer,1);
				ep->r = atoi(&epr);
				buffer++;

				//transfering eth_addr
				memcpy(epth->eth_addr,buffer,6);
				buffer=buffer+6;

				//transfer port
				char epthPort[100];
				bzero(epthPort,sizeof(epthPort));
				int i=0;
				while(*(buffer)!=0)
				{
					//test
					// printf("here\n");


					epthPort[i] = *(buffer);
					buffer++;
					i++;
				}
				epth->port = (uint32_t)atoi(epthPort);


				printf("serviceID: %s haddr: %s\n",ep->serviceID,epth->eth_addr);
				fp = fopen("eppm_cache","a");

				fprintf(fp, "%d,%s,%d,", -1,ep->serviceID,ep->r);
				
				// for(i = 0; i < 5; i++){
				// 	fprintf(fp,"%x:",*((epth->eth_addr)+i));
				// }	
				//test
				printf("testttttt%s\n",epth->eth_addr);
				fprintf(fp,"%s,",(epth->eth_addr));
				fprintf(fp,"%u\n",epth->port);

				fclose(fp);
				close(clientfd);
				break;
			}
			case 6:
			{
				fp = fopen("eppm_cache","r+");
				int fd = atoi(buffer);

				while(1)
				{	
					fscanf(fp,"%s",fileBuff);

					//end of file reached
					if(*fileBuff=='\0')
						break;

					token = strtok(fileBuff,",");	//tokenizing the string. Here token = smpfd
					int fd_cache = atoi(token);
					if(fd_cache == fd){
							int len = strlen(fileBuff);
							fseek(fp,-len,SEEK_CUR);
							fprintf(fp,"%d",-1);
							break;
					}

				}
                close(clientfd);
				fclose(fp);
				break;
			}	
			default:
			{
				printf("This should never happen\n");
				break;
			}

		}
		
		
	}

	/*---close socket---*/
	close(sockfd);
	return 0;
}

