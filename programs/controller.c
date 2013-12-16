/*
 *	CONTROLLER
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <resolv.h> 
#include <time.h>
#include <pthread.h>
#include <math.h>

#define lamp_step	3	// degrees interval to fire each lamp 
#define fan_step	0.5	// degrees interval to increase the fan speed of [fan_increment]
#define fan_increment	10

float current_temperature=0, target_temperature=0;
int   lamps=0, fan=0;
FILE* file;

pthread_t ctrl;
pthread_mutex_t mutex_temperature = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_actuators   = PTHREAD_MUTEX_INITIALIZER;


typedef struct
{
	int sock;
	struct sockaddr address;
	int addr_len;
} connection_t;

void * requestHandler(void * ptr);
void * controller(void * ptr);
void set_fan_speed(int val);
void set_lamps(int val);



int main(int argc, char ** argv)
{
	int port, sock=-1;
	struct sockaddr_in address;
	connection_t * connection;
	pthread_t thread;

	// check for command line arguments 
	if (argc != 2) {
		fprintf(stderr, "usage: %s port\n", argv[0]);
		return -1;
	}

	// obtain port number 
	if (sscanf(argv[1], "%d", &port) <= 0) { 
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	// create socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock <= 0) {
		fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
		return -3;
	}

	// bind socket to port
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "%s: error: cannot bind socket to port %d\n", argv[0], port);
		return -4;
	}

	// listen on port
	if (listen(sock, 5) < 0) {
		fprintf(stderr, "%s: error: cannot listen on port\n", argv[0]);
		return -5;
	}
	printf("\nCONTROLLER is ready and listening on port %i ..\n\n",port);

		
	// create the controller thread
	pthread_create(&ctrl,NULL,controller,NULL);
	

	while (1)
	{
		// accept incoming connections
		connection = (connection_t *)malloc(sizeof(connection_t));
		connection->sock = accept(sock, &connection->address, (socklen_t*)&connection->addr_len);
		if (connection->sock <= 0) {
			free(connection);
		}
		else {
			// start a new thread but do not wait for it 
			pthread_create(&thread, 0, requestHandler, (void *)connection);
			pthread_detach(thread);
		}
	}

	return 0;
}





/*
 *	Receive and parse a message and execute the requested action
 */
void * requestHandler(void * ptr)
{
	char buffer[255], reply[255], cmd[10], val[10];
	int len;
	connection_t * conn;
	if (!ptr) pthread_exit(0); 
	conn = (connection_t *)ptr;

	// read the meassage
	len = read(conn->sock,buffer,255);
	if (len > 0) {
		buffer[len] = 0;
		// printf("Received: %s\n",buffer);
		
		// parse the command
		char *token, *string, *tofree;
		string = strdup(buffer); tofree = string;
		token = strsep(&string, ";"); sprintf(cmd,"%s", token);
		token = strsep(&string, ";"); sprintf(val,"%s", token);
		free(tofree);


		// execute an action
		sprintf(reply,"cannot compute, unknown command!");

		if (strcmp(cmd, "SET") == 0) {
		
			// SET TARGET TEMPERATURE
			pthread_mutex_lock(&mutex_temperature);				
			target_temperature=atof(val);
			float diff=target_temperature-current_temperature;
			pthread_mutex_unlock(&mutex_temperature);
			if (diff>0) sprintf(reply,"Temperature is set! I have to INCREASE the box temp of %.1f degrees",diff);
			else sprintf(reply,"Temperature is set! I have to DECREASE the box temp of %.1f degrees",diff*-1);
		}

		if (strcmp(cmd, "TEMP") == 0) {
		
			// UPDATE CURRENT TEMPERATURE
			pthread_mutex_lock(&mutex_temperature);	
			current_temperature=atof(val);
			pthread_mutex_unlock(&mutex_temperature);	
			sprintf(reply,"Temperature value received!");
		}

		if (strcmp(cmd, "LOG") == 0) {
		
			// GENERATE A LOG LINE
			pthread_mutex_lock(&mutex_temperature);
			pthread_mutex_lock(&mutex_actuators);	
			sprintf(reply,"%.1f;%.1f;%d;%d",target_temperature,current_temperature,lamps,fan);
			pthread_mutex_unlock(&mutex_actuators);
			pthread_mutex_unlock(&mutex_temperature);
		}
		
		// send back a response
		write(conn->sock,reply,sizeof(reply));
	}

	// close socket and clean up 
	close(conn->sock);
	free(conn);
	pthread_exit(0);
}





/*
 *	Adjust fan speed and lamps to match the target desired temperature
 */
void * controller(void * ptr)
{
	int n;
	float diff;
	
	// turn on and off the lamps
	set_lamps(0); sleep(1);
	set_lamps(1); sleep(1);
	set_lamps(2); sleep(1);
	set_lamps(3); sleep(1);
	set_lamps(0);

	// ramp up the fan to 100
	for (n=0; n<5; n++) { set_fan_speed(fan+20); }

	// ramp down the fan to 25
	for (n=0; n<3; n++) { set_fan_speed(fan-20); }
	set_fan_speed(25);


	while(1){
		
		// delta temperature
		pthread_mutex_lock(&mutex_temperature);	
		diff=target_temperature-current_temperature;
		pthread_mutex_unlock(&mutex_temperature);
		

		// we need to RAISE the temperature
		if(diff>0) {
			// slow down the fan
			set_fan_speed(25);

			// turn on the lamps in a number proportional to the difference of temperature
			n = floor(diff/lamp_step)+1;
			if (n > 3) n = 3;
			set_lamps(n);
		}


		// we need to LOWER the temperature
		if(diff<0) {
			// turn off the lamps
			set_lamps(0);

			// speed up the fan to a number proportional to the difference of temperature
			n = -(floor(diff/fan_step)+1)*fan_increment;
			if (n > 100) { n = 100; }
			if (n < 25)  { n = 25;  }
			set_fan_speed(n);
		}


		sleep(1);
	}	
}



void set_fan_speed(int val) {

	pthread_mutex_lock(&mutex_actuators);
	fan=val;
	file = fopen("/dev/eprofan", "w");
	if (file != NULL) { fprintf(file, "%d", fan); fclose(file);}
	pthread_mutex_unlock(&mutex_actuators);
}

void set_lamps(int val) {

	pthread_mutex_lock(&mutex_actuators);
	lamps=val;
	file = fopen("/dev/microwave", "w");
	if (file != NULL) { fprintf(file, "%d", lamps); fclose(file);}
	pthread_mutex_unlock(&mutex_actuators);
}




