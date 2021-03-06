
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#define MAXDATASIZE 256
#define ERR_MSG "Not an available operation, try again.\n"

// enumerate connection states
// CONNECTED => connect function returned 
// ESTABLISHED => client has been named and identified
enum S_STATE{
    S_CONNECTED,
    S_ESTABLISHED,
    S_CLOSED
};

// enumerate search methods for linked list find function
enum SEARCH{
    NAME_SEARCH,
    SOCK_SEARCH
};

// enumerate chatroom functions
enum FUNCTION{
    BROADCAST,
    PRIVATE,
    LIST,
    ERROR
};

// struct to encapsulate client connection info in linked list node
typedef struct Connection{
    char* name;
    int socket, state;  
    struct Connection* next;
    struct Connection* prev;

} Connection;

// head and current ptrs for linked list
Connection* head_conn = NULL;
Connection* cur_conn = NULL;

// insert into linked list -- only inserts at head (that is all that is needed)
void
insert_conn(char* _name, int _socket, int _state)
{
    // create new node and fill in details
    Connection* new = (Connection*)malloc(sizeof(Connection));
    new->name = (char*)malloc(sizeof(char) * MAXDATASIZE);
    strcpy(new->name, _name);
    new->socket = _socket;
    new->state = _state;
    new->prev = NULL;

    // insert
    new->next = head_conn;
    if (new->next != NULL) new->next->prev = new;
    head_conn = new;
}

// find a connection by name or socket
// method = 0 => name
// method = 1 => socket
Connection*
find_conn(char* _name, int _socket, int method)
{
    Connection* cur = head_conn;

    // loop through linked list and find what we are looking for
    if (cur == NULL) return NULL;
    else if (method) 
    {
        while (cur != NULL)
        {
            if (cur->socket == _socket) return cur;
            cur = cur->next;
        }
    }
    else if (!method)
    {
       while (cur != NULL)
        {
            if (!strcmp(cur->name, _name)) return cur;
            cur = cur->next;
        } 
    }

    // not found
    return NULL;
}

// delete a single node
// can pass ptr to node for deletion, or name/socket
int
del_conn(Connection* node, char* _name, int _socket, int method)
{
    Connection* for_del;
    if (node == NULL)
    {
    // find the node
        for_del = find_conn(_name, _socket, method);
        if (for_del == NULL) return 1;
    }
    else
    {
        for_del = node;
    }

    // remove the node from linked list
    if (for_del->prev != NULL) for_del->prev->next = for_del->next;
    if (for_del->next != NULL) for_del->next->prev = for_del->prev;

    // delete
    if (head_conn == for_del) head_conn = for_del->next;
    for_del->prev = NULL;
    for_del->next = NULL;
    free(for_del->name);
    for_del->name = NULL;
    free(for_del);
    for_del = NULL;
    return(0);
}

// delete entire list
int
del_conn_list(){

    // delete head and keep moving down
    while (head_conn != NULL)
    {
        if (del_conn(head_conn, NULL, -1, -1)) return (1);
    }

    return (0);
}

// parse the msg from the client
int
parse_client_msg(char* buf, Connection* cur, int* rcv_sock)
{
    int len, op = -1;
    // copy buf and parse command
    char msg[MAXDATASIZE];
    strcpy(msg, buf);
    char cmd = msg[0];
    char* rcv_name;
    char* str;

    if (cmd == 'b')
    {
        // client wants to broadcast message
        op = BROADCAST;
        len = sprintf(buf, "Broadcast --- %s: %s\n", cur->name, &msg[2]);
        if (len < 0) 
        {
            perror("sprintf");
            exit(1);
        }
        buf[len] = '\0';
    }
    else if (cmd == 'u')
    {
        // client is identifying themselves after initial connection
        op = BROADCAST;

        // update client name and state, before updating buf
        strcpy(cur->name, &msg[2]);
        cur->state = S_ESTABLISHED;
        len = sprintf(buf, "User \"%s\" connected.\n", &msg[2]);
        if (len < 0) 
        {
            perror("sprintf");
            exit(1);
        }
        buf[len] = '\0';
    }
    else if (cmd == 'p')
    {
        op = PRIVATE;
        // intended receiver should be next string in message
        // find the rcv connection node by name and update socket
        // if not found ret error
        rcv_name = strtok(&msg[2], " ");
        Connection* rcv = find_conn(rcv_name, -1, NAME_SEARCH);
        if (rcv == NULL) return ERROR;
        *rcv_sock = rcv->socket;

        // actual chat message is location of rcv name + lenght of rcv name + 1 for null left behind by strtok
        str = (&msg[2] + strlen(rcv_name) + 1);
        len = sprintf(buf, "Private --- %s: %s\n", cur->name, str);
        if (len < 0) 
        {
            perror("sprintf");
            exit(1);
        }
        buf[len] = '\0';
    }
    else if (cmd == 'l')
    {
        op = LIST;
    }

    return op;
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
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv, op;

    struct addrinfo hints, *ai, *p;

    // parse command line args
    if (argc != 2) {
	       	fprintf(stderr,"usage: chatserver port#\n");
		exit(1);
	}

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "Failed to bind.\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // add the listener to connection list
    insert_conn("listener", listener, S_CONNECTED);    

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set

                        // add to conn_list
                        insert_conn("no name", newfd, S_CONNECTED);

                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("New connection from %s on "
                            "socket %d.\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {

                    // find corresponding node in linked list
                    Connection* cur = find_conn(NULL, i, SOCK_SEARCH);
                    
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("User \"%s\" on socket %d exited.\n", cur->name,i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set                
                        cur->state = S_CLOSED;
                        if (del_conn(cur, NULL, -1, -1)) 
                        {
                            perror("delete");
                            exit(1);
                        }                        
                    } else {
                        // we got some data from a client
                        
                        // parse buf here
                        buf[nbytes] = '\0';
                        int rcv_sock = -1;
                        op = parse_client_msg(buf, cur, &rcv_sock);
                        if (op == BROADCAST) {
                            // printf("Broadcast\n");
                            for(j = 0; j <= fdmax; j++) {
                                
                                // printf("New data from socket %d\n", i);
                                
                                if (FD_ISSET(j, &master)) {
                                    // except the listener and ourselves
                                    if (j != listener && j != i) {
                                        if (send(j, buf, strlen(buf), 0) == -1) {
                                            perror("send");
                                            exit(1);
                                        }
                                    }
                                }
                            }
                        }
                        else if (op == PRIVATE) {
                            // printf("PM\n");
                            if (send(rcv_sock, buf, strlen(buf), 0) == -1) {
                                perror("send");
                                exit(1);
                            }                     
                        }
                        else if (op == LIST) {
                            // printf("LIST\n");
                            sprintf(buf, "List of Active Users:\n");
                                if (send(i, buf, strlen(buf), 0) == -1) {
                                    perror("send");
                                    exit(1);
                                }   
                            Connection* cur = head_conn;
                            while (cur != NULL && cur->next != NULL){
                                sprintf(buf, "%s\n", cur->name);
                                if (send(i, buf, strlen(buf), 0) == -1) {
                                    perror("send");
                                    exit(1);
                                }   
                                cur = cur->next;                  
                            } 
                        }
                        else if (op == ERROR) {
                            // printf("ERR\n");
                            if (send(i, ERR_MSG, strlen(ERR_MSG), 0) == -1) {
                                perror("send");
                                exit(1);
                            }                     
                        }       
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    if(del_conn_list()) return 1;

    return 0;
}