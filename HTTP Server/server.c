#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include "threadpool.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

#define SERVER_HEADERS_v1 "Server: webserver/1.0\r\n"\
		"Content-Type: text/html\r\n"\
		"Connection: close\r\n"

#define SERVER_HEADERS_v2 "Server: webserver/1.0\r\n"\
		"Connection: close\r\n"

#define BODY_302 "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n"\
		"<BODY><H4>302 Found</H4>\r\n"\
		"Directories must end with a slash.\r\n"\
		"</BODY></HTML>\r\n"

#define BODY_400 "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n"\
		"<BODY><H4>400 Bad request</H4>\r\n"\
		"Bad Request.\r\n"\
		"</BODY></HTML>\r\n"

#define BODY_403 "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n"\
		"<BODY><H4>403 Forbidden</H4>\r\n"\
		"Access denied.\r\n"\
		"</BODY></HTML>\r\n"

#define BODY_404 "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n"\
		"<BODY><H4>404 Not Found</H4>\r\n"\
		"File not found.\r\n"\
		"</BODY></HTML>\r\n"

#define BODY_500 "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n"\
		"<BODY><H4>500 Internal Server Error</H4>\r\n"\
		"Some server side error.\r\n"\
		"</BODY></HTML>\r\n"

#define BODY_501 "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n"\
		"<BODY><H4>501 Not supported</H4>\r\n"\
		"Method is not supported.\r\n"\
		"</BODY></HTML>\r\n"

int isNumber(char* , int*);
char *get_mime_type(char *);
int handler (void *);
int response_to_client(int, int, char*, int);
int response_folder_to_client(int fd, char* );
int chcek_permissions(char* );

int main(int argc, char **argv) {
	int sockfd;					/* TCP socket descriptor */
	struct sockaddr_in srv;	    /* used by bind() */
	int *newfd;			        /* returned by accept() */
	int i , port , pool_size , num_of_requests;
	char *s_port , *s_pool_size , *s_num_of_requests;

	if(argc != 4){
		printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
		exit(1);
	}

	s_port = argv[1];
	s_pool_size = argv[2];
	s_num_of_requests = argv[3];

	if( !isNumber(s_port, &port) ||
			!isNumber(s_pool_size , &pool_size) ||
			!isNumber(s_num_of_requests , &num_of_requests)){
		printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
		exit(1);
	}

	/* First call to socket() function */
	if((sockfd = socket(PF_INET , SOCK_STREAM, 0)) < 0) {
		perror("ERROR opening socket");
		exit(1);
	}

	/* Initialize socket structure */
	bzero((char *) &srv, sizeof(srv));
	srv.sin_family = AF_INET;
	srv.sin_port = htons(port);
	srv.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &srv, sizeof(srv)) < 0) {
		perror("ERROR on binding");
		exit(1);
	}

	/* Now start listening for the clients,
	 * here process will go in sleep mode
	 * and will wait for the incoming connection.
	 */

	if(listen(sockfd, 5)  < 0) {
		perror("ERROR on listing");
		exit(1);
	}

	threadpool *pool = create_threadpool(pool_size);
	if(!pool)
		exit(1);

	for(i=0; i<num_of_requests; i++){
		newfd = (int*)malloc(sizeof(int));
		if(!newfd){
			perror("malloc");
			destroy_threadpool(pool);
			exit(1); // internal error!!
		}
		/* Accept actual connection from the client */
		if ((*newfd = accept(sockfd, NULL, NULL)) < 0) {
			perror("ERROR on accept");
			free(newfd);
			destroy_threadpool(pool);
			exit(1); // internal error!!
		}
		/* If connection is established then start communicating */
		dispatch(pool ,handler , (void*)newfd); // ??? need casting to void * ???
	}
	destroy_threadpool(pool);
	close(sockfd);
	return 0;
}

int handler(void *arg){
	char buff[4001];					/* used by read to get the 1st line request */
	char temp_path[4096];				/* temp char buffer.*/
	char full_path[4096];				/* full absolute server path.*/
	int nbytes = 0 , n;					/* used by read and write */
	int fd = *(int*)arg;				/* the socket file descriptor */
	char *tok , *method , *path, *ver;	/* pointers to the parameters in the first line request.*/
	struct stat s;						/* the file/folder properties */
	int file_fd;						/* if there is file on the request , this is the file descriptor.*/
	int res = 0 , perm;					/* res is for the return value from the response function, and perm is for the value from the check_permissions function.*/

	bzero(buff , sizeof(buff));
	while(1){
		if((n = read(fd, buff + nbytes, sizeof(buff)-nbytes-1)) < 0) {
			perror("ERROR reading from socket");
			return -1;
		}
		nbytes += n;
		if((tok = strstr(buff , "\r\n"))){
			*tok = '\0';
			break;
		}
	}
	/*
	 * Separate the first lint request to method , path and version.
	 */
	method = strtok(buff , " ");
	path = strtok(NULL , " ");
	ver = strtok(NULL , " ");

	/*
	 * Get the absolute server path and combined it with the request path.
	 */
	bzero(full_path, sizeof(full_path));
	strcat(full_path , ".");strcat(full_path , path);
	/*
	 * Now lets respond to the client
	 * 1. if one of the parameters in the request is missing -> replay 400 Bad request.
	 * 2. else if the version in the request is not HTTP/1.0 or HTTP/1.1 -> replay 400 Bad request.
	 * 3. else if the path is not starting with '/'
	 * 4. else if the method is not GET ->  replay 501 501 Not supported.
	 * --- if all above is right , lets try to understand the path.---
	 * 1. try to get information about the path(using stat).
	 * 	  1.1. if other users don't have permissions -> replay 403 Forbidden.
	 *    1.2. else if it folder:
	 *    	   1.2.1 if the path isn't ending with '/' -> replay 302 Found and return the correct location(with the '\')
	 *    	   1.2.2 else if there is index.html in this folder -> replay 200 and return the index.html.
	 *    	   1.2.3 else replay 200 and return HTML folder context.
	 *    1.3 else if it regular file -> replay 200 and return the file context.
	 *    1.4 else it is not regular file -> replay 403 Forbidden.
	 * 2. if you cannot getting the file information -> meaning it now exist replay 404 Not found.
	 *--- if error like , open file error , open folder error , or failed system calls -> replay 500 Internal server error.---
	 */

	if(!method || !path || !ver)
		res = response_to_client(fd, 400, NULL, 0);
	else if(strcmp(ver , "HTTP/1.1") != 0 && strcmp(ver , "HTTP/1.0") != 0)
		res = response_to_client(fd, 400, NULL, 0);
	else if(path[0] != '/')
		res = response_to_client(fd, 400, NULL, 0);
	else if(strcmp(method , "GET") != 0)
		res = response_to_client(fd, 501, NULL, 0);
	else if( !stat(full_path,&s) )
	{
		strcpy(temp_path , path+1);
		if((perm = chcek_permissions(temp_path)) <= 0){
			if(!perm)
				res = response_to_client(fd, 403, NULL, 0);
			else
				res = response_to_client(fd, 500, NULL, 0);
		}
		else if(S_ISDIR(s.st_mode))
		{
			if(path[strlen(path)-1] != '/')
				res = response_to_client(fd, 302, path, 0);
			else{
				bzero(temp_path , sizeof(temp_path));
				strcpy(temp_path , full_path);
				strcat(temp_path , "index.html");
				if( !stat(temp_path,&s) ){
					if(!(s.st_mode & S_IROTH))
						res = response_to_client(fd, 403, NULL, 0);
					else if((file_fd = open(temp_path , O_RDONLY)) < 0 ){
						perror("ERROR opening file");
						res = response_to_client(fd, 500, NULL , 0);
					}
					else {
						res = response_to_client(fd, 200, temp_path, file_fd);
						close(file_fd);
					}
				}
				else{
					if(ENOENT != errno){
						perror("stat");
						res = response_to_client(fd, 500, NULL, 0);
					}
					res = response_folder_to_client(fd,full_path);
				}
			}
		}
		else if(S_ISREG(s.st_mode))
		{
			if((file_fd = open(full_path , O_RDONLY)) < 0 ){
				perror("ERROR opening file");
				res = response_to_client(fd, 500, NULL , 0);
			}
			else{
				res = response_to_client(fd, 200, full_path, file_fd);
				close(file_fd);
			}
		}
		else
			res = response_to_client(fd, 403, NULL, 0);
	}
	else if(ENOENT == errno || errno == ENOTDIR)
		res = response_to_client(fd, 404, NULL, 0);
	else if(errno == EACCES)
		res = response_to_client(fd, 403, NULL, 0);
	else{
		perror("stat");
		res = response_to_client(fd, 500, NULL, 0);
	}
	close(fd);
	free(arg);
	return res;
}

/*
 * function that check if str is trying of numbers.
 * if such, placing the number as integer in num.
 */

int isNumber(char* str , int* num){
	int i;
	for(i = 0; i < strlen(str); i++)
		if(!isdigit(str[i]))
			return 0;
	*num = atoi(str);
	return 1;
}

/*
 * the function building folder response message to the client.
 * the format is as in file dir_content file.
 * parms :
 * fd - the socket file descriptor.
 * path- is the absolute server path + the requested path from the user.
 * dirp - is the pointer to the user requested folder.
 * return 0 on success , -1 on failure
 */

int response_folder_to_client(int fd, char* path){
	char buf[4096];
	char timebuf[128];
	int numOfFolders = 0 , length =0;
	struct stat s;
	struct dirent *dp;
	DIR *dirp1 , *dirp2; 				/* this is the folder pointers.*/

	if((dirp1 = opendir(path)) == NULL){
		perror("ERROR open dir");
		return response_to_client(fd, 500, NULL , 0);
	}
	if((dirp2 = opendir(path)) == NULL){
		perror("ERROR open dir");
		return response_to_client(fd, 500, NULL , 0);
	}

	/*
	 * sum the number of folders to numOfFolders.
	 */
	do {
		errno = 0;
		if ((dp = readdir(dirp1)) != NULL) {
			strcpy(buf , path);
			strcat(buf , dp->d_name);
			if( !stat(buf,&s) && dp->d_name[0] != '.')
				numOfFolders++;
			else{
				if(errno < 0){
					perror("stat");
					return response_to_client(fd, 500, NULL , 0);
				}
			}
		}
	} while (dp != NULL);

	if (errno != 0){
		perror("error reading directory");
		return response_to_client(fd, 500, NULL , 0);
	}

	length = (numOfFolders+1)*500 + 2*strlen(path);
	char *buffer = (char*)malloc(sizeof(char)*length);
	if(buffer == NULL){
		perror("malloc");
		return response_to_client(fd, 500, NULL , 0);
	}
	memset(buf , '\0' , sizeof(buf));
	memset(buffer , '\0' , length);

	/*
	 * Folder HTML body.
	 */
	strcat(buffer, "<HTML><HEAD><TITLE>Index of ");
	strcat(buffer, path);
	strcat(buffer, "</TITLE></HEAD>\n<BODY><H4>Index of ");
	strcat(buffer, path);
	strcat(buffer, "</H4>\n<table CELLSPACING=8>\n");
	strcat(buffer, "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\n\n");
	do {
		errno = 0;
		if ((dp = readdir(dirp2)) != NULL) {
			strcpy(buf , path);
			strcat(buf , dp->d_name);
			if( !stat(buf,&s) && dp->d_name[0] != '.'){
				strcat(buffer, "<tr>\n");
				strcat(buffer, "<td><A HREF=\"");
				strcat(buffer, dp->d_name);
				if(S_ISDIR(s.st_mode))
					strcat(buffer, "/");
				strcat(buffer, "\">");
				strcat(buffer, dp->d_name);
				if(S_ISDIR(s.st_mode))
					strcat(buffer, "/");
				strcat(buffer, "</A></td>\n<td>");
				strcat(buffer, ctime(&s.st_mtime));
				strcat(buffer, "</td>\n");
				if(S_ISREG(s.st_mode)){
					strcat(buffer, "<td>");
					memset(buf, '\0' , sizeof(buf));
					sprintf(buf, "%d", (int)s.st_size);
					strcat(buffer, buf);
					strcat(buffer, " Bytes</td>\n");
				}
				strcat(buffer, "</tr>\n");
			}
			else{
				if(errno < 0){
					perror("stat");
					return response_to_client(fd, 500, NULL , 0);
				}
			}
		}
	} while (dp != NULL);

	if (errno != 0){
		perror("error reading directory");
		return response_to_client(fd, 500, NULL , 0);
	}
	strcat(buffer, "</table>\n\n<HR>\n\n<ADDRESS>webserver/1.0</ADDRESS>\n</BODY></HTML>\n");


	/*
	 * send the headers.
	 */
	time_t now;
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	strcat(timebuf , "\r\n");
	//timebuf holds the correct format of the current time.

	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(fd, buf, strlen(buf), 0);
	strcpy(buf, "Date: ");
	send(fd, buf, strlen(buf), 0);
	send(fd, timebuf, strlen(timebuf), 0);
	strcpy(buf, "Last-Modified: ");
	send(fd, buf, strlen(buf), 0);
	send(fd, timebuf, strlen(timebuf), 0);
	send(fd, SERVER_HEADERS_v1, strlen(SERVER_HEADERS_v1), 0);
	strcpy(buf, "Content-Length: ");
	send(fd, buf, strlen(buf), 0);
	sprintf(buf, "%d", (int)strlen(buffer));
	send(fd, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(fd, buf, strlen(buf), 0);

	strcpy(buf, "\r\n");
	send(fd, buf, strlen(buf), 0);

	/*
	 * send the body.
	 */
	int nBytes = 0 , n = 0;
	while(nBytes != (int)strlen(buffer)){
		if((n = write(fd, buffer + nBytes, (int)strlen(buffer)-nBytes)) < 0) {
			perror("ERROR reading from socket");
			return -1;
		}
		nBytes += n;
	}
	closedir(dirp1);
	closedir(dirp2);
	free(buffer);
	return 0;
}


/*
 * the function building response message to the client.
 * first sending the headers in case of each status.
 * secondly sending the shared headers - server name , date , content-type etc.
 * third building the body in case of each status.
 * --- in case of status 200 we using to read from the file_fd descriptor and write it to the socket(fd)---
 * parms:
 * fd- is the socket file descriptor.
 * path - is the absolute server path + the requested path from the user. can be NULL if no path.
 * file_fd - is the file, file descriptor, the specific file in the end of the path.
 * return 0 on success , -1 on failure
 */
int response_to_client(int fd , int status, char* path, int file_fd){
	char timebuf[128];
	char buf[4096];
	int nBytes = 0 , n = 0;

	bzero(buf, sizeof(buf));
	time_t now;
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	strcat(timebuf , "\r\n");
	//timebuf holds the correct format of the current time.

	switch(status){
	case 302:
		strcpy(buf, "HTTP/1.0 302 Found\r\n");
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Content-Length: 123\r\n");
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Location: ");
		send(fd, buf, strlen(buf), 0);
		strcat(path, "/\r\n");
		send(fd, path, strlen(path) , 0);
		break;
	case 400:
		strcpy(buf, "HTTP/1.0 400 Bad Request\r\n");
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Content-Length: 113\r\n");
		send(fd, buf, strlen(buf), 0);
		break;
	case 403:
		strcpy(buf, "HTTP/1.0 403 Forbidden\r\n");
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Content-Length: 111\r\n");
		send(fd, buf, strlen(buf), 0);
		break;
	case 404:
		strcpy(buf, "HTTP/1.0 404 Not Found\r\n");
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Content-Length: 112\r\n");
		send(fd, buf, strlen(buf), 0);
		break;
	case 500:
		strcpy(buf, "HTTP/1.0 500 Internal Server Error\r\n");
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Content-Length: 144\r\n");
		send(fd, buf, strlen(buf), 0);
		break;
	case 501:
		strcpy(buf, "HTTP/1.0 501 Not supported\r\n");
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Content-Length: 129\r\n");
		send(fd, buf, strlen(buf), 0);
		break;
	default: // STATUS 200 OK -  header of file
		strcpy(buf, "HTTP/1.0 200 OK\r\n");
		send(fd, buf, strlen(buf), 0);
		char* type = get_mime_type(path);
		if(type){
			strcpy(buf, "Content-Type: ");
			send(fd, buf, strlen(buf), 0);
			send(fd, type, strlen(type), 0);
			strcpy(buf, "\r\n");
			send(fd, buf, strlen(buf), 0);
		}
		struct stat s;
		stat(path , &s);
		strcpy(buf, "Content-Length: ");
		send(fd, buf, strlen(buf), 0);
		sprintf(buf, "%d\r\n", (int)s.st_size);
		send(fd, buf, strlen(buf), 0);
		strcpy(buf, "Last-Modified: ");
		send(fd, buf, strlen(buf), 0);
		send(fd, timebuf, strlen(timebuf), 0);
		break;
	}
	if(status == 200)
		send(fd, SERVER_HEADERS_v2, strlen(SERVER_HEADERS_v2), 0);
	else
		send(fd, SERVER_HEADERS_v1, strlen(SERVER_HEADERS_v1), 0);

	strcpy(buf, "Date: ");
	send(fd, buf, strlen(buf), 0);
	send(fd, timebuf, strlen(timebuf), 0);

	strcpy(buf, "\r\n");
	send(fd, buf, strlen(buf), 0);

	// BODY
	switch(status){
	case 302:
		send(fd, BODY_302, sizeof(BODY_302), 0);
		break;
	case 400:
		send(fd, BODY_400, sizeof(BODY_400), 0);
		break;
	case 403:
		send(fd, BODY_403, sizeof(BODY_403), 0);
		break;
	case 404:
		send(fd, BODY_404, sizeof(BODY_404), 0);
		break;
	case 500:
		send(fd, BODY_500, sizeof(BODY_500), 0);
		break;
	case 501:
		send(fd, BODY_501, sizeof(BODY_501), 0);
		break;
	default:
		/* STATUS - 200 OK
		 * sending the file data.
		 */
		while ((nBytes = read(file_fd, buf, sizeof(buf))) > 0) {
			if((n = write(fd, buf, nBytes)) < 0){
				perror("write");
				return -1;
			}
			bzero(buf, sizeof(buf));
		}
		if (nBytes < 0){
			perror("read");
			return -1;
		}
		break;
	}
	return 0;
}

/*
 * the function check if the others users have permissions to the path.
 * to each folder in the path they should have executable
 * and to the file they should have read.
 * return 1 on success(have permissions) , 0 on failed(don't have permissions), -1 on system failure.
 */

int chcek_permissions(char* path){
	char* curr;
	struct stat s;
	char absolute_path[4096];
	bzero(absolute_path, sizeof(absolute_path));
	strcat(absolute_path, ".");

	curr = strtok(path , "/");
	while(curr != NULL){
		strcat(absolute_path , "/");
		strcat(absolute_path , curr);
		if(stat(absolute_path,&s) < 0)
			return -1;
		else if(S_ISDIR(s.st_mode)){
			if(!(s.st_mode & S_IXOTH))
				return 0;
		}
		else if(S_ISREG(s.st_mode)){
			if(!(s.st_mode & S_IROTH))
				return 0;
		}
		else
			return 0;
		curr = strtok(NULL , "/");
	}
	return 1;
}

char *get_mime_type(char *name)
{
	char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mp4") == 0) return "video/mp4";
	return NULL;
}
