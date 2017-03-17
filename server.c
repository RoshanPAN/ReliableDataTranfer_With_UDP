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

#define SENDER_PORT "14952"	// the port for server to send data
#define RCV_PORT "14951"  // the port for server to receive msg (NOT used)

#define MAXBUFLEN 1000
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
     * (rcv_port::Integer)\n
     * (seq_no::Integer)\r\n
     * (MsgContent::char/binary)
    */

#define FILE_REQUEST 1
#define FILE_TRANSFER 2
#define ACK 3
#define NAK 4
#define MISSION_OVER 5
#define ERR 6  // file not exists and so on




/* When sending Msg */
int buildMsg(char *msg, int msg_type, int port_num, int seq_no,char *content){

    // Put whole header + msg content into msg
    int len = 0;
    char header[50];
    bzero(header, sizeof header);
    sprintf(header, "%d\n%.5d\n%d\r\n", msg_type, port_num, seq_no);
    len = len + strlen(header) + 1;
    char tmp[2000];
    bzero(tmp, sizeof tmp);
    strcpy(tmp, header);
    strcat(tmp, content);
    len = len + strlen(content) + 1;
    bzero(msg, sizeof msg);
    strcpy(msg, tmp);
    return len;
}

int buildFileRequestMsg(char *msg, int my_rcv_port_num, char *file_name){
    int len = buildMsg(msg, 1, my_rcv_port_num, 0, file_name);
    return len;
}

int buildFileTransferMsg(char *msg, int my_rcv_port_num, int seq_no,char *file_chunk){
    int len =  buildMsg(msg, FILE_TRANSFER, my_rcv_port_num, seq_no, file_chunk);
    return len;
}

void buildACKMsg(char *msg, int my_rcv_port_num, int seq_no, int next_chunk_id){
    char chunk_id_str[10];
    sprintf(chunk_id_str, "%d", next_chunk_id);
    buildMsg(msg, ACK, my_rcv_port_num, seq_no, chunk_id_str);
}

void buildNAKMsg(char *msg, int my_rcv_port_num, int seq_no, int next_chunk_id){
    char chunk_id_str[10];
    sprintf(chunk_id_str, "%d", next_chunk_id);
    buildMsg(msg, NAK, my_rcv_port_num, seq_no,chunk_id_str);
}



/* When Receiving Msg */
int parseMsgType(char* buf)
{
    char num = buf[0];
    char tmp[2];
    tmp[0] = num;
    tmp[1] = '\0';
    return atoi(tmp);
}

void parseMsgContent(char *msg, char *msg_content)
{
    char *tmp;
    tmp = strstr(msg, "\r\n");
    tmp = tmp + 2;
    strcpy(msg_content, tmp);
}

/*
 * Helper function for parseFilePath
 */
void buildFilePath(char *absolute_path, char *file_name){
    char path[100];
    if (getcwd(path, sizeof(path)) == NULL)
        fprintf(stdout, "fail to get cwd: %s\n", path);
    strcat(path, "/");
    strcat(path, file_name);
    strcpy(absolute_path, path);
}

void parseFilePath(char *msg, char *absolute_path){

    char file_name[100];
    parseMsgContent(msg, file_name);
    buildFilePath(absolute_path, file_name);
}

void parseTheirRcvPort(char *msg, char *their_rcv_port){
    char *tmp;
    tmp = strstr(msg, "\n");
    tmp = tmp + 1;
    //TODO  cut the port number = length 5
    strncpy(their_rcv_port, tmp, 5);
    their_rcv_port[5] = '\0';
}

int parseSequenceNumber(char *msg){
    char *tmp;
    tmp = strstr(msg, "\n");
    tmp = tmp + 1;
    tmp = strstr(tmp, "\n");
    tmp = tmp + 1;
    char num[5];
    bzero(num, sizeof num);
    strncpy(num, tmp, 1);
    num[1] = '\0';
    return atoi(num);
}




void buildPathToRcvFolder(char *absolute_path, char *file_name){
    char path[100];
    if (getcwd(path, sizeof(path)) == NULL)
        fprintf(stdout, "fail to get cwd: %s\n", path);
    strcat(path, "/");
    strcat(path, "ReceivedFile/");
    strcat(path, file_name);
    strcpy(absolute_path, path);
}


void appendToFile(char *file_name, char *data_buf){
    char absolute_path[100];
    buildPathToRcvFolder(absolute_path, file_name);
//    printf(absolute_path);
    FILE *fd = NULL;
    fd = fopen(absolute_path, "a");
    if(fd){
        fputs(data_buf, fd);
        fclose(fd);
    }else{
        perror("\n[Received File] During opening ");
    }
}

/* msg format:
     * (MsgType::Integer)\n
     * (rcv_port::Integer)\n
     * (seq_no::Integer)\r\n
     * (MsgContent::char/binary)
    */





int main(int argc, char *argv[])
{
	int sockfd_rcv, sockfd_snd;
	struct addrinfo hints, *servinfo, *p;
    struct addrinfo hints2, *servinfo2, *p2;
	int rv, rv2;
	int numbytes;
	struct sockaddr_in client_addr;
	char buf[MAXBUFLEN];
	char s[INET6_ADDRSTRLEN];
    char ipstr[INET6_ADDRSTRLEN];
    char *rcv_port;


    /* variables during processing client request */
    int msg_type = -1;
    char *file_name = NULL;
    char *file_path = NULL;
    char *tmp1 = NULL;
	FILE *fd;

    if (argc != 2) {
        fprintf(stderr,"Server: Incorrect Arguments Counts!\n");
        exit(1);
    }

    /* read stdin for port number */
    rcv_port = argv[1];


    while(1)
    {
        /*
         * Setp 1:
         * Create socket for Receiver on Port: rcv_port
         */
        memset(&hints, 0, sizeof hints);
        memset(&servinfo, 0, sizeof servinfo);
        memset(&p, 0, sizeof p);

        hints.ai_family = AF_INET;//AF_UNSPEC; // set to AF_INET to force IPv4
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP (AI_PASSIVE  tells getaddrinfo() to assign the address of my local host to the socket structures)

        if ((rv = getaddrinfo(NULL, rcv_port, &hints, &servinfo)) != 0) {
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


            if ((sockfd_rcv = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1) {
                perror("listener: socket");
                continue;
            }

            if (bind(sockfd_rcv, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd_rcv);
                perror("listener: bind");
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "listener: failed to bind socket\n");
            return 2;
        }else{
            printf("\nServer: listen for client request on (port number: %s)...\n", rcv_port);
        }



        // Step 2: wait for the Request message
        // if msg type != 1, print error msg
        // else, then go to Step2: enter Finite State Machine to start transfer file

        memset(buf, 0, MAXBUFLEN);
        int addr_len = sizeof(client_addr);
        numbytes = recvfrom(sockfd_rcv, buf, MAXBUFLEN - 1, 0,
                            (struct sockaddr *) &client_addr, (socklen_t *) &addr_len);
        if (numbytes == -1) {
            perror("listener: error in receiving client request");
            exit(1);
        }
        fflush(stdout);
        char client_PORT[10];
        parseTheirRcvPort(buf, client_PORT);
        char *client_IP = inet_ntoa(client_addr.sin_addr);
        printf("\nClient IP & Port: %s:%s", client_IP, client_PORT);

        msg_type = parseMsgType(buf);
        printf("\nMessage Type:%d", msg_type);


        //TODO
//        /* to be deleted */
//        strcpy(buf, "1\n2\n3\r\nLICENSE");
//        /* to be deleted */



        if (msg_type != 1) {
            perror("the client request is not valid.\n");
            sleep(1);
            continue;
        } else {
            file_path = (char *) malloc(100);
            parseMsgContent(buf, file_path);
            printf("\nPath for requested file:%s\n\n", file_path);
            // read file
            fd = fopen(file_path, "r");
            if (fd == NULL) {
                printf("File open error, %s!", strerror(errno));
                continue;
            }
        }


        /*
         * Step  3:
         * Create Sender socket
         */
        /* set parameter for getaddrinfo */
        memset(&hints2, 0, sizeof hints2);

        hints2.ai_family = AF_INET;
        hints2.ai_socktype = SOCK_DGRAM;
        // "getaddrinfo" will do the DNS lookup & return
        if ((rv2 = getaddrinfo(client_IP, client_PORT, &hints2, &servinfo2)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv2));
            return 1;
        }

        // loop through all the results and make a socket
        for(p2 = servinfo2; p2 != NULL; p2 = p2->ai_next) {

            /*************Print Info********************/
            void *addr;
            char *ipver;

            // get the pointer to the address itself,
            // different fields in IPv4 and IPv6:
            if (p2->ai_family == AF_INET) { // IPv4
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)p2->ai_addr;
                addr = &(ipv4->sin_addr);
                ipver = "IPv4";
            } else { // IPv6
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p2->ai_addr;
                addr = &(ipv6->sin6_addr);
                ipver = "IPv6";
            }

            // convert the IP to a string and print it:
            inet_ntop(p2->ai_family, addr, ipstr, sizeof ipstr);
            printf("  %s: %s\n", ipver, ipstr);
            /************************************/

            if ((sockfd_snd = socket(p2->ai_family, p2->ai_socktype,
                                 p2->ai_protocol)) == -1) {
                perror("Client: socket creation ");
                continue;
            }

            break;
        }
        printf("Send Destination IP: %s, Send Destination Port: %s, Requested File: %s\n", client_IP, client_PORT, file_name);

        if (p == NULL) {
            fprintf(stderr, "Sender: failed to bind socket\n");
            return 2;
        }

        /* Step 4:
         * enter Finite State Machine to start transfer file.
         */
        /* file processing */
        char file_buf[MAXBUFLEN];
        char msg_buf[MAXBUFLEN * 2];
        /* FSM workflow control */
        int nread, chunk_id = 1;
        bool is_chunk_complete = false;

        bzero(file_buf, sizeof file_buf);
        printf("Sender Start ::");
        sleep(0.1);
        int seq_no = 0;
        while ((nread = fread(file_buf, 1, sizeof file_buf - 100, fd)) > 0) {
            printf("\n[Sending]  chunk_id:%d    length:%d\n", chunk_id, nread);
            sleep(0.05);
            //TODO sequence number
            int msg_len = buildFileTransferMsg(msg_buf, atoi(rcv_port), seq_no,file_buf);
            printf(msg_buf);
            if ((numbytes = sendto(sockfd_snd, msg_buf, sizeof msg_buf, 0,
                                   p2->ai_addr, p2->ai_addrlen)) == -1) {
                perror("[Sender]: Failed to send ");
                is_chunk_complete = false;
            } else {
                is_chunk_complete = true;
            }

            bzero(file_buf, sizeof file_buf);
            if (is_chunk_complete) {
                chunk_id++;
            }
        }

        /* Step 5:
         * Upon finish, send a MISSION_OVER Message
         */
        bzero(msg_buf, sizeof msg_buf);
        buildMsg(msg_buf, MISSION_OVER, atoi(rcv_port), seq_no, "\r\n<FINISH>\r\n");
        if ((numbytes = sendto(sockfd_snd, msg_buf, strlen(msg_buf), 0,
                               p2->ai_addr, p2->ai_addrlen)) == -1) {
            perror("[Sender]: MISSION_OVER Message ");
        }



        sleep(0.05);

        close(sockfd_rcv);
        close(sockfd_snd);
        printf("\n ----- Finish File Sending ---- \n");



//    printf("listener: waiting for client msg (recvfrom)...\n");
//
//	addr_len = sizeof(their_addr);
//	if ((numbytes = recvfrom(sockfd_rcv, buf, MAXBUFLEN-1 , 0,
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

    }


	return 0;
}
