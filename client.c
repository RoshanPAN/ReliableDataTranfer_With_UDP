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
#define MAXBUFLEN 900

#define INITIAL_STATE 0
#define CONFIRM 7
#define FILE_REQUEST 1
#define FILE_TRANSFER 2
#define ACK 3
#define NAK 4
#define MISSION_OVER 5
#define ERR 6  // file not exists and so on

typedef int bool;
#define true 1
#define false 0



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
	int sockfd_snd, sockfd_rcv;
	struct addrinfo hints, *servinfo, *p;
    struct addrinfo hints2, *servinfo2, *p2;
	int rv;
	int rv2;
	int numbytes;
    char ipstr[INET6_ADDRSTRLEN];

    /* user input */
    char *server_port = NULL;
    char *file_name = NULL;
    char *server_IP = NULL;
    char rcv_port[10];


    /* read stdin for port number */
	if (argc != 4) {
		fprintf(stderr,"Client: Not Enough Arguments!\n");
		exit(1);
	}
    server_IP = argv[1];
    server_port = argv[2];
    file_name = argv[3];
    sprintf(rcv_port, "%d", (atoi(server_port) + 10));
    printf("[STDIN]  Server IP: %s, Server Listening Port: %s, Requested File: %s\n", server_IP, server_port, file_name);
    printf("[Client] Client Listening Port: %s\n", rcv_port);


    /*
     * Setp 1:
     * Create socket for Sending to Port: server_IP, server_port
     */
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
        
		if ((sockfd_snd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("[Client] sending socket creation ");
			continue;
		}

		break;
	}
    printf("[Client] Sender IP: %s, Sender Port: %s, Requested File: %s\n", server_IP, server_port, file_name);

	if (p == NULL) {
		fprintf(stderr, "Sender: socket for send msg \n");
		return 2;
	}

    /*
     * Step 2:
     * Send File Request Msg.
     */
    // Build Request Msg
    char msg[MAXBUFLEN * 2];
    buildFileRequestMsg(msg, atoi(rcv_port),file_name);
    if ((numbytes = sendto(sockfd_snd, msg, strlen(msg), 0,
			 p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Client: try send file request message to server ");
		exit(1);
	}
    printf("----Request Msg-----\n%s\n----------\n", msg);
	printf("Client: Request Msg Sent for  %s:%s length: numbytes: %d\n", server_IP, server_port, numbytes);


    /*
     * Step 3:
     * create rcv socket for receiving file from server: rcv_port
     * Receive Msg.
     */
    memset(&hints2, 0, sizeof hints2);


    hints2.ai_family = AF_INET;//AF_UNSPEC; // set to AF_INET to force IPv4
    hints2.ai_socktype = SOCK_DGRAM;
    hints2.ai_flags = AI_PASSIVE; // use my IP (AI_PASSIVE  tells getaddrinfo() to assign the address of my local host to the socket structures)

    if ((rv2 = getaddrinfo(NULL, rcv_port, &hints2, &servinfo2)) != 0) {  // bind to any local IP & given port
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv2));
        return 1;
    }

    // have a look at the servname(port) and IP binded
//    char hostname[100], servname[100];
//    getnameinfo(servinfo2->ai_addr, servinfo2->ai_addrlen, hostname, sizeof hostname, servname, sizeof servname, NI_NUMERICSERV);
//    printf("%s", hostname);
//    struct sockaddr_in *a = (struct sockaddr_in *) servinfo2->ai_addr;
//    struct sockaddr_in *b = (struct sockaddr_in *) servinfo->ai_addr;
//    printf("\n%d, %s", a->sin_port, rcv_port);


    // loop through all the results and bind to the first we can
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
        printf(" %s: %s\n", ipver, ipstr);
        /************************************/


        if ((sockfd_rcv = socket(p2->ai_family, p2->ai_socktype,
                                 p2->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd_rcv, p2->ai_addr, p2->ai_addrlen) == -1) {
            close(sockfd_rcv);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p2 == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    } else{
        printf("[Receiving] Receiving Socket created & binded.\n");
    }

    /*
     * Step 4:
     * Receive Msg.
     */
    /* file processing */
    char data_buf[MAXBUFLEN + 100];
    char msg_buf[MAXBUFLEN * 2];
    /* FSM workflow control */
    int nread, chunk_id, expected_seq_no, seq_no;
    bool iscomplete = false;
    struct sockaddr_in server_addr;
    int addr_len = sizeof(server_addr);
    int current_state = INITIAL_STATE;

    while(1){
        fflush(stdout);
        // Finite State Machine
        switch (current_state)
        {
            case INITIAL_STATE:
                // Reset buffer
                memset(msg_buf, 0, sizeof msg_buf);
                bzero(data_buf, sizeof data_buf);
                // Receive msg
                numbytes = recvfrom(sockfd_rcv, msg_buf, MAXBUFLEN - 1, 0,
                                    (struct sockaddr *) &server_addr, (socklen_t *) &addr_len);
                // Redirect the FSM into certain state
                chunk_id ++;
                switch (parseMsgType(msg_buf)){
                    case FILE_TRANSFER:
//                        chunk_id = 1;
//                        expected_seq_no = 0;
                        current_state = FILE_TRANSFER; break;
                    case ERR:
                        current_state = ERR; break;
                    case MISSION_OVER:
                        current_state = MISSION_OVER; break;
                    default:
                        current_state = ERR; break;
                }
                break;
            case FILE_TRANSFER:
                parseMsgContent(msg_buf, data_buf);
                seq_no = parseSequenceNumber(msg);
                //TODO add logic for rdt, but now, just receive and write into file
                printf("\n\n\n[Receiving] Receivd chunk #%d, length %d.\n ", chunk_id, numbytes);
                appendToFile(file_name, data_buf);
                printf(data_buf);
                chunk_id ++;
                // Reset buffer
                memset(msg_buf, 0, sizeof msg_buf);
                bzero(data_buf, sizeof data_buf);
                // Receive msg
                numbytes = recvfrom(sockfd_rcv, msg_buf, sizeof msg_buf, 0,
                                    (struct sockaddr *) &server_addr, (socklen_t *) &addr_len);
                switch (parseMsgType(msg_buf)){
                    case FILE_TRANSFER:
                        current_state = FILE_TRANSFER; break;
                    case ERR:
                        current_state = ERR; break;
                    case MISSION_OVER:
                        current_state = MISSION_OVER; break;
                    default:
                        current_state = ERR; break;
                }

                break;
            case MISSION_OVER:
                iscomplete = true;
                break;

        }
        if(iscomplete) break;


    }


    freeaddrinfo(servinfo);
    close(sockfd_rcv);
	close(sockfd_snd);

	return 0;
}
