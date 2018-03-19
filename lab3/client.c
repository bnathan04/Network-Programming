
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

/********** NEW CODE START **********/
#include <sys/time.h>
#define MAXBUFLEN 1000
#define NUM_MSG 2

#define SHORT_MSG "Hello World!"
#define LONG_MSG "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras interdum dui non libero egestas vestibulum. Duis velit leo, feugiat sed ante id, efficitur gravida"


// get sockaddr, IPv4 or IPv6:
void*
get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// calculate time elapsed given two timeval structs
double
time_elapsed(struct timeval a, struct timeval b)
{
	double start = ((double)a.tv_sec)*1000000 + (double)a.tv_usec; 
	double end = ((double)b.tv_sec)*1000000 + (double)b.tv_usec;

	return ((end - start)/1000000); 
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	double numbytes_sent[2], numbytes_recv[2], t_naught[2], t_prop, bit_rate;

	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	addr_len = sizeof their_addr;
	struct timeval start_time, end_time;
	char* test_msgs[2] = {SHORT_MSG, LONG_MSG};

	if (argc != 3) {
		fprintf(stderr,"usage: talker hostname port#\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("talker: socket");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "talker: failed to create socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	for (int i = 0; i < NUM_MSG; ++i)
	{
		
		if(gettimeofday(&start_time, NULL) != 0) return(-1);

		if ((numbytes_sent[i] = sendto(sockfd, test_msgs[i], strlen(test_msgs[i]), 0,
				 p->ai_addr, p->ai_addrlen)) == -1) {
			perror("talker: sendto");
			exit(1);
		}
		
		if ((numbytes_recv[i] = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}

		// get end time here
		if(gettimeofday(&end_time, NULL) != 0) return(-1);

		// terminate the string received in the buffer
		int index = numbytes_recv[i];
		buf[index] = '\0';
		
		// do calculation
		t_naught[i] = time_elapsed(start_time, end_time);
		
	}


	double sent_one = numbytes_sent[0]*8;
	double sent_two = numbytes_sent[1]*8;
	bit_rate = (2*sent_two - 2*sent_one)/(t_naught[1] - t_naught[0]);
	t_prop = (t_naught[0]/2) - (sent_one/bit_rate);
	
	// printf("To1: %lf, To2: %lf, Bytes1: %lf, Bytes2: %lf\n", t_naught[0], t_naught[1], sent_one, sent_two);
	printf("Prop Delay: %lf seconds, Bit Rate: %lf bps\n", t_prop, bit_rate);

	close(sockfd);

	return 0;
}