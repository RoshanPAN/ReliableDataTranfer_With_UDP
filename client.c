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
#include <time.h>

#define SERVERPORT "4950"	// the port users will be connecting to
/* msg format:
     * (MsgType::Integer)\n
     * (NotUsed::Integer)\n
     * (NotUsed::Integer)\r\n
     * (MsgContent::char/binary)
    */
#define MAXBUFLEN 900


#define FILE_REQUEST 1
#define FILE_TRANSFER 2
#define ACK0 3
#define ACK1 4
#define MISSION_OVER 5
#define ERR 6  // file not exists and so on

typedef int bool;
#define true 1
#define false 0

/* package loss probability k% */
#define LOSS_PROB 20

/* FSM States */
#define WAIT_FOR_0 0
#define WAIT_FOR_1 1



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

void buildACK0Msg(char *msg, int my_rcv_port_num, int seq_no, int next_chunk_id){
    char chunk_id_str[10];
    sprintf(chunk_id_str, "%d", next_chunk_id);
    buildMsg(msg, ACK0, my_rcv_port_num, seq_no, chunk_id_str);
}

void buildACK1Msg(char *msg, int my_rcv_port_num, int seq_no, int next_chunk_id){
    char chunk_id_str[10];
    sprintf(chunk_id_str, "%d", next_chunk_id);
    buildMsg(msg, ACK1, my_rcv_port_num, seq_no,chunk_id_str);
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



ssize_t myRecvfromWithLoss(int __fd, void *__buf, size_t __n, int __flags, struct sockaddr *__addr, socklen_t *__addr_len)
{
    ssize_t numbytes;
    int rand_num = rand() % 100;
    printf("\n Rand num: %d \n", rand_num);
    if( rand_num < LOSS_PROB){
        numbytes = recvfrom(__fd, __buf, __n, __flags, (struct sockaddr *)__addr, (socklen_t *)__addr_len);
    } else{
        return -1;
    }

}


//numbytes = recvfrom(sockfd_rcv, msg_buf, sizeof msg_buf, 0,
//                    (struct sockaddr *) &server_addr, (socklen_t *) &addr_len);



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
    srand(time(NULL));  // random feeds

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
    char in_msg_buf[MAXBUFLEN * 2];
    char out_msg_buf[MAXBUFLEN];
    /* FSM workflow control */
    int nread, pkt_id = 1, expected_seq_no, seq_no;
    bool iscomplete = false;
    struct sockaddr_in server_addr;
    int addr_len = sizeof(server_addr);
    int current_state = WAIT_FOR_0;
    int is_loss = false;
    int n = 0;
    while(!iscomplete)
    {
        n++;
        memset(in_msg_buf, 0, sizeof in_msg_buf);
        bzero(data_buf, sizeof data_buf);
        bzero(out_msg_buf, sizeof out_msg_buf);
        // Finite State Machine
        switch (current_state)
        {
            case WAIT_FOR_0:
                // Receive msg with loss (back to receiving state when loss)
                do{
                    numbytes = recvfrom(sockfd_rcv, in_msg_buf, sizeof in_msg_buf, 0,
                                        (struct sockaddr *) &server_addr, (socklen_t *) &addr_len);

                    if((rand() % 100) < LOSS_PROB) {
                        printf("[Receiving] Package #%d Loss. \n", pkt_id);
                        fflush(stdout);
                        is_loss = true;
                    }else{
                        is_loss = false;
                    }
                }while(is_loss);
                printf(in_msg_buf);
                if(parseMsgType(in_msg_buf) == MISSION_OVER){
                    iscomplete = true;
                    break;
                }

                //logic for rdt3.0 based on seq_no
                seq_no = parseSequenceNumber(msg);
                switch (seq_no)
                {
                    case 0:
                        // write into file
                        parseMsgContent(in_msg_buf, data_buf);
                        appendToFile(file_name, data_buf);
                        printf("[WAIT_FOR_0] Received Packet #%d, length %d, seq no.: %d .\n ", pkt_id, numbytes, seq_no);
                        // Send ACK 0
                        buildACK0Msg(out_msg_buf, atoi(rcv_port), seq_no, pkt_id + 1);
                        if ((numbytes = sendto(sockfd_snd, out_msg_buf, sizeof out_msg_buf, 0,
                                               p->ai_addr, p->ai_addrlen)) == -1) {
                            printf("[WAIT_FOR_0] Failed to send ACK%d to server.\n", seq_no);
                        }else{
                            printf("[WAIT_FOR_0] ACK%d is sent to server.\n", seq_no);
                        }
                        pkt_id ++;
                        current_state = WAIT_FOR_1;
                        break;
                    case 1:
                        // send ACK 1
                        bzero(out_msg_buf, sizeof out_msg_buf);
                        buildACK1Msg(out_msg_buf, atoi(rcv_port), seq_no, pkt_id + 1);
                        if ((numbytes = sendto(sockfd_snd, out_msg_buf, sizeof out_msg_buf, 0,
                                               p->ai_addr, p->ai_addrlen)) == -1) {
                            printf("[WAIT_FOR_0] Failed to send ACK%d to server.\n", seq_no);
                        }else{
                            printf("[WAIT_FOR_0] ACK%d is sent to server.\n", seq_no);
                        }
                        current_state = WAIT_FOR_0;
                }
                break;

            case WAIT_FOR_1:
                // Reset buffer
                bzero(in_msg_buf, sizeof in_msg_buf);
                bzero(data_buf, sizeof data_buf);
                bzero(out_msg_buf, sizeof out_msg_buf);
                // Receive msg with loss (back to receiving state when loss)
                do{
                    numbytes = recvfrom(sockfd_rcv, in_msg_buf, sizeof in_msg_buf, 0,
                                        (struct sockaddr *) &server_addr, (socklen_t *) &addr_len);

                    if((rand() % 100) < LOSS_PROB) {
                        printf("[WAIT_FOR_1] Package #%d Loss. \n", pkt_id);
                        fflush(stdout);
                        is_loss = true;
                    }else{
                        is_loss = false;
                    }
                }while(is_loss);
                printf("[WAIT_FOR_1]\n");
                printf(in_msg_buf);
                if(parseMsgType(in_msg_buf) == MISSION_OVER){
                    iscomplete = true;
                    break;
                }

                //logic for rdt3.0 based on seq_no
                seq_no = parseSequenceNumber(in_msg_buf);
                switch (seq_no)
                {
                    case 0:
                        // Send ACK 0
                        bzero(out_msg_buf, sizeof out_msg_buf);
                        buildACK0Msg(out_msg_buf, atoi(rcv_port), seq_no, pkt_id + 1);
                        if ((numbytes = sendto(sockfd_snd, out_msg_buf, sizeof out_msg_buf, 0,
                                               p->ai_addr, p->ai_addrlen)) == -1) {
                            printf("[WAIT_FOR_1] Failed to send ACK%d to server.\n", seq_no);
                        }else{
                            printf("[WAIT_FOR_1] ACK%d is sent to server.\n", seq_no);
                        }
                        current_state = WAIT_FOR_1;
                        break;
                    case 1:
                        // write into file
                        parseMsgContent(in_msg_buf, data_buf);
                        appendToFile(file_name, data_buf);
                        printf("[WAIT_FOR_1] Received Packet #%d, length %d, seq no.: %d .\n ", pkt_id, numbytes, seq_no);
                        pkt_id ++;
                        // send ACK 1
                        buildACK1Msg(out_msg_buf, atoi(rcv_port), seq_no, pkt_id + 1);
                        if ((numbytes = sendto(sockfd_snd, out_msg_buf, sizeof out_msg_buf, 0,
                                               p->ai_addr, p->ai_addrlen)) == -1) {
                            printf("[WAIT_FOR_1] Failed to send ACK%d to server.\n", seq_no);
                        }else{
                            printf("[WAIT_FOR_1] ACK%d is sent to server.\n", seq_no);
                        }
                        current_state = WAIT_FOR_0;
                }
                break;
        } // end of swtich case

    } // end of while
    printf("Total number of packages received: %d", n);

    freeaddrinfo(servinfo);
    close(sockfd_rcv);
	close(sockfd_snd);

	return 0;
}
