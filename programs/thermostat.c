/*
 *	THERMOSTAT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h> 


int main(int argc, char *argv[])
{
	int sockfd,n;
	struct sockaddr_in serv_addr;
	char msg[512], buf[512]; 

	if(argc != 3) {
		printf("\n Usage: %s <server ip> <server port>\n",argv[0]);
		return 1;
	}


	// Message sending loop
	while(1)
	{
		// Open connection with server
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			printf("\n Error : Could not create socket \n");
			return -1;
		} 
		memset(&serv_addr, '0', sizeof(serv_addr)); 
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(atoi(argv[2])); 

		if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0) {
			printf("\n inet_pton error occured\n");
			return -1;
		} 

		// WAIT for a connection with the controller
		while( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			printf("Controller is not ready, waiting 5 sec \n\n");
			sleep(5);
		} 
	
	
		// read value from user
		printf("\n > Enter a new target temperature [C]: ");
        	scanf("%s" , buf);

		// create command
		sprintf(msg,"SET;%s",buf);

		// Send value
		if( send(sockfd , msg , strlen(msg) , 0) < 0) {
		    puts("Send failed");
		    return 1;
		}

		n = read(sockfd,buf,512);
		if (n < 0) { perror("ERROR reading response"); exit(1); }
		printf("   server reply: %s\n",buf);


		// close
		close(sockfd); 	
	}
	
	return 0;
}
	
