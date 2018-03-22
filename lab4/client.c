
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define BROADCAST "broadcast"
#define LIST "list"
#define EXIT "exit"

// double check the max data size limitations
#define MAXDATASIZE 256 // max number of bytes we can get at once 

// send username
void
identify(int sockfd, char* name)
{
    char* buf[MAXDATASIZE];
    int len = sprintf(buf, "u %s", name);
    if (len < 0)
    {
        perror("sprintf");
        exit(1);
    }
    buf[len] = '\0';

    if (send(sockfd, buf, strlen(buf), 0) == -1)
    {
        perror("send");
        exit(1);
    }
}

// function to handle input parsing
void
parse_input(char* buf)
{
    char* str;
    char output[MAXDATASIZE];
    int len;
    // copy input to new string 
    char input[MAXDATASIZE];
    strcpy(input, buf);
    // identify the command 
    char* cmd = strtok(input, " ");
    
    // set the output to correct codes for parsing in server    
    if (strcmp(cmd, BROADCAST) == 0)
    {
        // strtok leaves behind a null terminator at the point it finds the delimiter
        // take advantage of this to extract the message which is right after the delimiter 
        str = input;            
        do
        { 
            str++;
        }
        while (*(str - 1) != '\0');

        printf("str result: %s\n", str);
        len = sprintf(buf, "b %s", str); 
        if (len < 0) 
        {
            perror("sprintf");
            exit(1);
        }
        buf[len] = '\0';
    }
    else if (strcmp(cmd, LIST) == 0)
    {
        buf[0] = 'l';
        buf[1] = '\0';
    }
    else if (strcmp(cmd, EXIT) == 0)
    {
        buf[0] = 'x';
        buf[1] = '\0';
    }
    else // client assumes that anything else is a valid username for a pm
    {
        str = input;            
        do
        { 
            str++;
        }
        while (*(str - 1) != '\0');

        len = sprintf(buf, "p %s %s", cmd, str);
        if (len < 0) 
        {
            perror("sprintf");
            exit(1);
        }
        buf[len] = '\0';
    }
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes, i;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    fd_set master, read_fds;
    int fd_max;

    // parse command line arguments
    if (argc != 4) {
        fprintf(stderr,"usage: chatclient [server address] [server port] [username]\n");
        exit(1);
    }

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    // send username to server
    identify(sockfd, argv[3]);

    freeaddrinfo(servinfo); // all done with this structure

    // add the server and stdin to the set and keep track of fdmax
    FD_SET(0, &master);
    FD_SET(sockfd, &master);
    fd_max = sockfd;
    
    // main loop
    while (1)
    {
        read_fds = master;
        if (select(fd_max+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        for ( i = 0; i < fd_max + 1; i++)
        {
            if(FD_ISSET(i, &read_fds))
            {
                if (!i) // stdin
                {
                    // handle input from commmand line
                    fgets(buf, MAXDATASIZE - 1, stdin);
                    
                    // parse input here
                    parse_input(buf);

                    // printf("Parsed input: %s\n", buf);

                    // send parsed message to server
                    if (send(sockfd, buf, strlen(buf), 0) == -1)
                    {
                        perror("send");
                        exit(1);
                    }
                    // printf("Sent user input to server.\n");
                } 
                else
                {
                    // either server or another client with private messaging
                    if ((numbytes = recv(i, buf, MAXDATASIZE-1, 0)) == -1)
                    {
                        perror("recv");
                        exit(1);
                    }

                    // terminate the string and output
                    buf[numbytes] = '\0';
                    printf("Received from socket %d:\n", i);
                    printf("%s\n", buf);
                }
            }
        }
    }
    // if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
    //     perror("recv");
    //     exit(1);
    // }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);

    close(sockfd);

    return 0;
}