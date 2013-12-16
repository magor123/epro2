/*
 *	SENSOR
 *
 *	Read the temperature value from the I2C sensor and send it to the controller
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

float t=0;
void  read_temperature();


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
	
	
		// read temperature value from sensor
		read_temperature();
		printf("\n > I2C temperature sensor value [C]: %.1f \n",t);

		// create command
		sprintf(msg,"TEMP;%.1f",t);

		// Send value to controller
		if( send(sockfd , msg , strlen(msg) , 0) < 0) {
		    puts("Send failed");
		    return 1;
		}

		n = read(sockfd,buf,512);
		if (n < 0) { perror("ERROR reading response"); exit(1); }
		printf("   server reply: %s\n",buf);


		// close
		close(sockfd);

		// sleep
		sleep(1);
	}
	
	return 0;
}



void read_temperature() {

	// Read temperature from temperature sensor

	t=24.6;

}
	
