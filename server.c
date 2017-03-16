/*
** server.c -- a datagram sockets "server" demo
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

#define MYPORT "4950"	// the port users will be connecting to

#define MAXBUFLEN 1025
#define PATH_LEN 100

typedef int bool;
#define true 1
#define false 0

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* msg format:
     * (MsgType::Integer)\n
     * (NotUsed::Integer)\n
     * (NotUsed::Integer)\r\n
     * (MsgContent::char/binary)
    */

#define FILE_REQUEST 1
#define FILE_TRANSFER 2
#define ACK 3
#define NAK 4

/* When sending Msg */
void buildMsg(char *msg, int msg_type, char *content){

    // Put whole header + msg content into msg
    char header[50];
    bzero(header, sizeof header);
    sprintf(header, "%d\n0\n0\r\n", msg_type);

    char tmp[2050];
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
	struct sockaddr_in client_addr;
	char buf[MAXBUFLEN];
	char s[INET6_ADDRSTRLEN];
    char ipstr[INET6_ADDRSTRLEN];
    char *portno;


    /* variables during processing client request */
    int msg_type = -1;
    char *file_name = NULL;
    char *file_path = NULL;
    char *tmp1 = NULL;
	FILE *fd;



    /* read stdin for port number */
    portno = argv[1];

    if(portno == NULL) {
		portno = (char *) malloc(sizeof MYPORT);
		memset(portno, 0, sizeof portno);
		strcpy(portno, MYPORT);
	}
	printf("%s", portno);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;//AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP (AI_PASSIVE  tells getaddrinfo() to assign the address of my local host to the socket structures)

	if ((rv = getaddrinfo(NULL, portno, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
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
        printf(" %s: %s\n", ipver, ipstr);
        /************************************/
        
        
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

    // Step 1: wait for the 1st message
    // if msg type != 1, print error msg
	// else, then go to Step2: enter Finite State Machine to start transfer file
    printf("listener: waiting for client request (port number: %s)...\n", portno);

	memset(buf, 0, MAXBUFLEN);
    int addr_len = sizeof(client_addr);
    numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                        (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
    if (numbytes == -1) {
        perror("listener: error in receiving client request");
        exit(1);
    }
    fflush(stdout);
    int client_PORT = ntohs(client_addr.sin_port);
    char* client_IP = inet_ntoa(client_addr.sin_addr);
    printf("\nClient IP & Port: %s:%d", client_IP, client_PORT);

    msg_type = parseMsgType(buf);
	printf("\nMessage Type:%d", msg_type);


	//TODO
	/* to be deleted */
	strcpy(buf, "1\n2\n3\r\nLICENSE");
	/* to be deleted */




    if(msg_type != 1){
        perror("the client request is not valid.\n");
        exit(1);
    }else{
        file_path = (char *)malloc(100);
        parseMsgContent(buf, file_path);
		printf("\nPath for requested file:%s\n\n", file_path);
		// read file
        fd = fopen(file_path, "r");
        if(fd == NULL){
            printf("File open error, %s!", strerror(errno));
            printf("fd:%d", fd);
        }
    }

    /* Step2:
     * enter Finite State Machine to start transfer file.
     */
    /* file processing */
    char file_buf[MAXBUFLEN];
    char msg_buf[MAXBUFLEN * 2];
    /* FSM workflow control */
    int nread, chunk_id=1;
    bool is_chunk_complete= false;

    bzero(file_buf, sizeof file_buf);
    printf("Sender Start ::");
    nread = fread(file_buf, 1, sizeof file_buf, fd);

    while ((nread = fread(file_buf, 1, sizeof file_buf, fd)) > 0){
        printf("\n    [Sending]  chunk_id:%d    length:%d", chunk_id, nread);
        buildFileTransferMsg(msg_buf, file_buf);
        if ((numbytes = sendto(sockfd,  msg_buf, strlen(msg_buf), 0,
                               p->ai_addr, p->ai_addrlen)) == -1) {
            perror("[Sender]: Failed to send ");
            is_chunk_complete = false;
        }else{
            is_chunk_complete = true;
        }

        bzero(file_buf, sizeof file_buf);
        if(is_chunk_complete) chunk_id++;
    }







//    printf("listener: waiting for client msg (recvfrom)...\n");
//
//	addr_len = sizeof(their_addr);
//	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
//                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
//		perror("recvfrom");
//		exit(1);
//	}
//
//
//	printf("listener: got packet from %s\n",
//		inet_ntop(their_addr.ss_family,
//			get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));
//	printf("listener: packet is %d bytes long\n", numbytes);
//	buf[numbytes] = '\0';
//	printf("listener: packet contains \"%s\"\n", buf);

    freeaddrinfo(servinfo);
	close(sockfd);

	return 0;
}
