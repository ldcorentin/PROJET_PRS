#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "functions.h"

struct timeval RTTtimeval;

// Function sameConsecutiveACK() which test if all ACK are equals
int nullACK(int tabACK[], int size){
	int i;
	for(i=0; i<size; i++){
		if(tabACK[i] != -1){
			return 0;
		}
	}
	return 1;
}

// Function max which find the maximum value in a table
int max(int tab[], int size){
	int i;
	int val_max = 0;
	for (i=0; i<size; i++){
		if (tab[i]>val_max) val_max=tab[i];
	}
	return val_max;
}

// Function refreshBuffer which replaces all the char of a char[] with a '\0'
void refreshBuffer(char buf[], int size){
	int i = 0;
	for(i = 0; i<size; i++){
		buf[i]='\0';
	}
}

void resetTIMEVAL(struct timeval *start, struct timeval *end){
	(*start).tv_sec=0;
	(*start).tv_usec=0;
	(*end).tv_sec=0;
	(*end).tv_usec=0;
}

void startRTT(struct timeval *start, struct timezone *tz){
	gettimeofday(start, tz);
}

void endRTT(struct timeval *end, struct timezone *tz, struct timeval *start, struct timeval *RTTtimeval){
	gettimeofday(end, tz);
	timersub(end, start, RTTtimeval);
	printf("RTTtimeval : %ld usec\n", (*RTTtimeval).tv_usec);
}

// Function receiveACK_Segment, return -1 if nothing received, 0 if 'FIN' reserve, or ACK if received
int receiveACK_Segment(char bufferACK[], int desc, struct sockaddr_in adressClient, int* sizeResult, fd_set set, struct timeval *RTTtimeval, struct timeval *waiting_time){
	int i;
	char numACK[7];
	// Waiting on the socket
	FD_SET(desc, &set);
	//timeradd(RTTtimeval, RTTtimeval, waiting_time); // RTT*2 to detect paquet loss
	select(desc+1, &set, NULL, NULL, RTTtimeval);
	if(FD_ISSET(desc, &set)){
		recvfrom(desc, bufferACK, 11, 0, (struct sockaddr*)&adressClient, sizeResult);
		bufferACK[11]='\0';
		for(i=0; i<6; i++){
			numACK[i] = bufferACK[i+3];
		}
		numACK[6]='\0';
		return atoi(numACK);
	}
	return -1;
}

// Test if we've got the port number into the arguments
void testArg(int* argc){
	if(*argc<2){
		printf("Please enter the server's port in argument\n");
		exit(0);
	}
}

//Socket creation
void openSocketUDP(int* desc){
	*desc=socket(AF_INET, SOCK_DGRAM, 0);
	if(*desc<0){
		printf("Error socket creation\n");
	}
	int one = 1;
	setsockopt(*desc, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
}

//init adress
void editStructurAdress(struct sockaddr_in* ptrAdress, int port, int ip){
	ptrAdress->sin_family = AF_INET;
	ptrAdress->sin_port = htons(port);
	ptrAdress->sin_addr.s_addr = htonl(ip);
}


//bind adress
int bindServer(int* soc, struct sockaddr_in* ptrAdress){
	int b=bind(*soc, (struct sockaddr*)ptrAdress, sizeof(*ptrAdress));
	if(b==-1){
		printf("bind error\n");
		close(*soc);
		return -1;
	}
}

// Function sendData : send a paquet with a size of RCVSIZE, and the 6 first bytes are the segment number
int sendData(int seq, char buffer[], char purData[], int desc, struct sockaddr_in adressClient, socklen_t adressClientLength, int sizeBuffer){ //purData 1018, buffer 1024.
	int i, try;
	char zero[1], c[7];
	c[6] = '\0';
	sprintf(zero, "%d", 0);
	if(seq<10){
		for(i=0; i<5; i++) buffer[i]='0';
		buffer[i]='\0';
	}else if(9<seq && seq<100){
		for(i=0; i<4; i++) buffer[i]='0';
		buffer[i]='\0';
	}else if(99<seq && seq<1000){
		for(i=0; i<3; i++) buffer[i]='0';
		buffer[i]='\0';
	}else if(999<seq && seq<10000){
		for(i=0; i<2; i++) buffer[i]='0';
		buffer[i]='\0';
	}else if(9999<seq && seq<100000){
		for(i=0; i<1; i++) buffer[i]='0';
		buffer[i]='\0';
	}
	sprintf(c, "%d", seq);
	strcat(buffer, c);
	buffer[6]='\0';
	int compteurTest;
	for(compteurTest=0; compteurTest<RCVSIZE-6; compteurTest++)	buffer[compteurTest+6] = purData[compteurTest];
	//printf("%d %d %d %d %s\n", desc, adressClient.sin_port, adressClientLength, adressClient.sin_family, inet_ntoa(adressClient.sin_addr));
	try = sendto(desc, buffer, sizeBuffer, 0, (struct sockaddr*)&adressClient, adressClientLength);
	if(try<0){
		perror("error sendto");
	}
	return try;
}

//Send a SYNACK message
void sendSYNACK(int* desc, struct sockaddr_in* ptrAdress, int new_port){
	char bufferSYNACK[12] = "SYN-ACK\0";
	char bufferPort[5] = "0000\0";
	sprintf(bufferPort, "%d", new_port);
	strcat(bufferSYNACK, bufferPort);
	printf("%s\n", bufferSYNACK);
	socklen_t sizePtrAdress = sizeof(*ptrAdress);
	sendto(*desc, bufferSYNACK, sizeof(bufferSYNACK), 0, (struct sockaddr*)ptrAdress, sizePtrAdress);
}

//handshake for the server
int handShakeServer(int* desc, struct sockaddr_in* ptrAdress, int port_data){
	char bufferSYN[4]="SYN\0";
	char bufferACK[4]="ACK\0";
	char buffer[10];
	socklen_t sizePtrAdress = sizeof(*ptrAdress);

	int msg=recvfrom(*desc, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)ptrAdress, &sizePtrAdress);

	if(strcmp(buffer,bufferSYN)==0){
		// recv SYN
		printf("SYN received\n");
		// Send SYN-ACK
		printf("Send SYN-ACK\n");
		sendSYNACK(desc, ptrAdress, port_data);
		// recv ACK
		recvfrom(*desc, buffer, sizeof(buffer), 0, (struct sockaddr*)ptrAdress, &sizePtrAdress);
		if(strcmp(buffer,bufferACK)==0){
			printf("ACK received\n");
			printf("Connexion established : Ok\n");
			return 1;
		}else{
			printf("Failed : ACK didn't received\n");
			close(*desc);
			return -1;
		}
	}else{
		printf("Failed : SYN didn't received\n");
		close(*desc);
		return -1;
	}
	return -1;
}


// function handleError
void handleError(int val, char* error){
	if(val<0){
		printf("Error : %s\n", error);
		exit(0);
	}
}

// Function receiveFileName
void receiveFileName(int descData, struct sockaddr_in adressClient, char fileName[])
{
	refreshBuffer(fileName, 100);
	socklen_t size = sizeof(adressClient);
	//printf("Waiting for the filename...\n");
	recvfrom(descData, fileName, 100, 0, (struct sockaddr*)&adressClient, &size);
	//printf("the file's name is : %s\n", (fileName));
}
