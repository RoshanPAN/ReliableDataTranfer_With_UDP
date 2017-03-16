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



int parseMsgType(char* buf)
{
    char num = buf[0];
    printf("%c", num);
    char tmp[2];
    tmp[0] = num;
    tmp[1] = '\0';
    return atoi(tmp);
}

void parseFilePath(char *buf, char *file_path)
{
    char *tmp;
    char path[100];
    if (getcwd(path, sizeof(path)) == NULL)
        fprintf(stdout, "fail to get cwd: %s\n", path);
//    printf(buf);
//    printf("-----------\n");
    tmp = strstr(buf, "\r\n");
    tmp = tmp + 2;
//    printf("\ntmp:%s", tmp);
    strcat(path, "/");
    strcat(path, tmp);
    strcpy(file_path, path);
}


int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
    char ipstr[INET6_ADDRSTRLEN];
    char *portno;


    /* variables during processing client request */
    int msg_type = -1;
    char *file_name = NULL;
    char *file_path = NULL;
    char *tmp1 = NULL;
	FILE *fd;

    /* file processing */
    char file_buf[MAXBUFLEN];

    /* FSM workflow control */
    bool is_complete = false;

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
	printf("########## 1 ########");

	freeaddrinfo(servinfo);
    // Step 1: wait for the 1st message
    // if msg type != 1, print error msg
	// else, then enter Finite State Machine to start transfer file
    printf("listener: waiting for client request (port number: %s)...\n", portno);
	printf("########## 2 ########");

	memset(buf, 0, MAXBUFLEN);
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("listener: error in receiving client request");
        exit(1);
    }
	addr_len = sizeof(their_addr);


    msg_type = parseMsgType(buf);
	printf("########## %d, msg_type ########\n", msg_type);





	//TODO
	/* to be deleted */
	strcpy(buf, "1\n2\n3\r\n.gitignore");
	/* to be deleted */

    if(msg_type != 1){
        perror("the client request is not valid.\n");
        exit(1);
    }else{
		printf("shit1\n");
        file_path = (char *)malloc(100);
        parseFilePath(buf, file_path);
		printf("\nPath for requested file:%s\n\n", file_path);
		// read file
        fd = fopen(file_path, "rb");
        if(fd == NULL){
            printf("File open error, %s!", strerror(errno));
            printf("fd:%d", fd);
        }
    }

    int nread;
    bzero(file_buf, sizeof file_buf);
    while ((nread = fread(file_buf, 1, sizeof file_buf, fd)) > 0){
        printf("%s", file_buf);
        bzero(file_buf, sizeof file_buf);
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

	close(sockfd);

	return 0;
}
