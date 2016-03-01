#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include <ctype.h>
#include "slist.h"

#define BUFSIZE 4096

void sig_handler(int);
int isNumber(char* , int* );
void touppercases(char*);

slist_t *clients;
int sockfd;
slist_destroy_t clients_data = SLIST_FREE_DATA;

struct request{
	char buf[BUFSIZE];
	struct sockaddr_in client_addr;
};
typedef struct request t_request;

int main(int argc, char **argv) {
	int port , nbytes ;
	struct sockaddr_in srv;			/* socket address   */
	fd_set readfds , writefds;		/* used by select() */
	int n_select;					/* num of active fd */
	t_request *new_request;			/* struct of new client request */

	/* check command line arguments */
	if (argc != 2) {
		fprintf(stderr,"usage: %s <port>\n" , argv[0]);
		exit(0);
	}
	/* check port argument */
	if(!isNumber(argv[1] , &port)){
		fprintf(stderr,"usage: %s <port>\n" , argv[0]);
		exit(0);
	}

	/* socket: create the UDP socket */
	if((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
		perror("ERROR opening socket");
		exit(0);
	}

	/* bind: use the Internet address family and any of my address*/
	srv.sin_family = AF_INET;
	srv.sin_port = htons(port);
	srv.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sockfd, (struct sockaddr*) &srv, sizeof(srv)) < 0){
		perror("ERROR on binding");
		close(sockfd);
		exit(0);
	}

	clients = (slist_t*)malloc(sizeof(slist_t));
	if(!clients){
		perror("ERROR on malloc");
		close(sockfd);
		exit(0);
	}
	slist_init(clients);
	signal(SIGINT, sig_handler);

	while(1){
		FD_ZERO(&readfds); FD_ZERO(&writefds);
		FD_SET(sockfd, &readfds);

		if(!slist_size(clients)){ // no requests!!
			printf("Server is ready for read\n");
			n_select = select(sockfd+1, &readfds, NULL, NULL, NULL);
		}
		else{
			printf("Server is ready for write\n");
			FD_SET(sockfd, &writefds);
			n_select = select(sockfd+1, &readfds, &writefds, NULL, NULL);
		}
		if(n_select < 0){
			perror("select");
			close(sockfd);
			exit(0);
		}
		if(FD_ISSET(sockfd, &readfds)){
			new_request = (t_request*)malloc(sizeof(t_request));
			if(!new_request){
				perror("malloc");
				close(sockfd);
				slist_destroy(clients , clients_data);
				exit(0);
			}
			memset(new_request->buf, '\0', BUFSIZE);
			socklen_t cli_len = sizeof(new_request->client_addr);
			if ((nbytes = recvfrom(sockfd, new_request->buf, BUFSIZE, 0,
					(struct sockaddr*) &(new_request->client_addr), &cli_len)) < 0){
				perror("recvfrom");
				close(sockfd);
				slist_destroy(clients , clients_data);
				exit(0);
			}
			touppercases(new_request->buf);
			slist_append(clients, new_request);
		}
		if(FD_ISSET(sockfd, &writefds)){
			t_request *req = slist_pop_first(clients);
			if((nbytes = sendto(sockfd, req->buf, strlen(req->buf), 0,
					(struct sockaddr*) &(req->client_addr), sizeof(req->client_addr))) < 0){
				perror("sendto");
				close(sockfd);
				slist_destroy(clients , clients_data);
				exit(0);
			}
			free(req);
		}
	}
	slist_destroy(clients , clients_data);
	close(sockfd);
	return 0;
}
void touppercases(char* str){
	int i;
	for(i=0; i<strlen(str); i++)
		str[i] = toupper(str[i]);
}

int isNumber(char* str , int* num){
	int i;
	for(i = 0; i < strlen(str); i++)
		if(!isdigit(str[i]))
			return 0;
	*num = atoi(str);
	return 1;
}

void sig_handler(int sig){
	if(sig == SIGINT){
		close(sockfd);
		slist_destroy(clients , clients_data);
		exit(0);
	}
}
