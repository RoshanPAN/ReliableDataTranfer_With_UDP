/*
** client.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVERPORT "4950"	// the port users will be connecting to
/* msg format:
     * (MsgType::Integer)\n
     * (NotUsed::Integer)\n
     * (NotUsed::Integer)\r\n
     * (MsgContent::char/binary)
    */
#define MAXBUFLEN 1025
#define FILE_REQUEST 1
#define FILE_TRANSFER 2
#define ACK 3
#define NAK 4

typedef int bool;
#define true 1
#define false 0

/* When sending Msg */
void buildMsg(char *msg, int msg_type, char *content){

    // Put whole header + msg content into msg
    char header[50];
    bzero(header, sizeof header);
    sprintf(header, "%d\n0\n0\r\n", msg_type);

    char tmp[2000];
    bzero(tmp, sizeof tmp);
    strcpy(tmp, header);
    strcat(tmp, content);
    bzero(msg, sizeof msg);
    strcpy(msg, tmp);
}

void buildFileRequestMsg(char *msg, char *file_name){
    buildMsg(msg, FILE_REQUEST, file_name);
}

void buildFileTransferMsg(char *msg, char *file_chunk){
    //...
    buildMsg(msg, FILE_TRANSFER, file_chunk);
}

void buildACKMsg(char *msg, int next_chunk_id){
    char chunk_id_str[10];
    sprintf(chunk_id_str, "%d", next_chunk_id);
    buildMsg(msg, ACK, chunk_id_str);
}

void buildNAKMsg(char *msg, int next_chunk_id){
    char chunk_id_str[10];
    sprintf(chunk_id_str, "%d", next_chunk_id);
    buildMsg(msg, NAK, chunk_id_str);
}


/* When Receiving Msg */
int parseMsgType(char* buf)
{
    char num = buf[0];
    printf("%c", num);
    char tmp[2];
    tmp[0] = num;
    tmp[1] = '\0';
    return atoi(tmp);
}

void parseMsgContent(char *msg, char *msg_content)
{

    char *tmp;
    char path[100];
    if (getcwd(path, sizeof(path)) == NULL)
        fprintf(stdout, "fail to get cwd: %s\n", path);
    tmp = strstr(msg, "\r\n");
    tmp = tmp + 2;
    strcat(path, "/");
    strcat(path, tmp);
    strcpy(msg_content, path);
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
    char ipstr[INET6_ADDRSTRLEN];

    /* user input */
    char *server_port = NULL;
    char *file_name = NULL;
    char *server_IP = NULL;

	if (argc != 4) {
		fprintf(stderr,"Client: Not Enough Arguments!\n");
		exit(1);
	}

	/* read stdin for port number */
    server_IP = argv[1];
    server_port = argv[2];
	if(server_port == NULL) server_port = SERVERPORT;
    file_name = argv[3];
    printf("Sender IP: %s, Sender Port: %s, Requested File: %s", server_IP, server_port, file_name);


    /* set parameter for getaddrinfo */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
    // "getaddrinfo" will do the DNS lookup & return
	if ((rv = getaddrinfo(server_IP, server_port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}


	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
        
        /*************Print Info********************/
        void *addr;
        char *ipver;
        
        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }
        
        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("  %s: %s\n", ipver, ipstr);
        /************************************/
        
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("Client: socket creation ");
			continue;
		}

		break;
	}
//    printf("Sender IP: %s, Sender Port: %s, Requested File: %s\n", server_IP, server_port, file_name);

	if (p == NULL) {
		fprintf(stderr, "Client: failed to bind socket\n");
		return 2;
	}

    /*
     * Step 1:
     * Send File Request Msg.
     */
    // Build Request Msg
    char *msg[MAXBUFLEN * 2];
    buildFileRequestMsg(msg, file_name);
    if ((numbytes = sendto(sockfd, msg, strlen(msg), 0,
			 p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Client: try send file request message to server ");
		exit(1);
	}
    printf("----Request Msg-----\n%s\n----------\n", msg);
	printf("Client: Request Msg Sent for  %s:%s length: numbytes: %d\n", server_IP, server_port, numbytes);

    /*
     * Step 2:
     * Receive Msg.
     */
    /* file processing */
//    char file_buf[MAXBUFLEN];
//    char msg_buf[MAXBUFLEN * 2];
//    /* FSM workflow control */
//    int nread, chunk_id=1;
//    bool is_chunk_complete= false;
//    File *fd;
//
//    bzero(file_buf, sizeof file_buf);
//    printf("Receiver Start ::");
//    nread = fread(file_buf, 1, sizeof file_buf, fd);
//
//    while ((nread = fread(file_buf, 1, sizeof file_buf, fd)) > 0){
//        printf("\n    [Sending]  chunk_id:%d    length:%d", chunk_id, nread);
//        buildFileTransferMsg(msg_buf, file_buf);
//        if ((numbytes = sendto(sockfd,  msg_buf, strlen(msg_buf), 0,
//                               p->ai_addr, p->ai_addrlen)) == -1) {
//            perror("[Sender]: Failed to send ");
//            is_chunk_complete = false;
//        }else{
//            is_chunk_complete = true;
//        }
//
//        bzero(file_buf, sizeof file_buf);
//        if(is_chunk_complete) chunk_id++;
//    }


    freeaddrinfo(servinfo);
	close(sockfd);

	return 0;
}
