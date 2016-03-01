#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/unistd.h>
#include <sys/socket.h>


#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
typedef int bool;
#define TRUE 1
#define FALSE 0

/*
 * struct for the URL storage
 */
struct parsed_url {
    char *host;                 /* mandatory */
    char* port;       		    /* optional */
    char *path;                 /* optional */
};
typedef struct parsed_url url_t;

/*
 * struct for the flags storage
 * urlFlag - define if a URL is exist
 * d - define if a -d flag is exist
 * h - define if a -h flag is exist
 * url - pointer to a URL structure.
 * time interval- pointer to the inserted time interval.
 */
struct flags{
	bool urlFlag;
	bool d;
	bool h;
	struct parsed_url *url;
	char* time_interval;
};
typedef struct flags flags_t;

flags_t* parseFlags(int length , char** );
url_t* parse_url(char *);
void parsed_url_free(url_t *);
void parsed_flags_free(flags_t *);
char* get_time_interval(char *);


int main(int argc, char **argv) {

	flags_t *flags = parseFlags(argc , argv);

	int fd;					/* socket descriptor */
	/* create the socket */
	if((fd = socket(PF_INET , SOCK_STREAM, 0)) < 0){
		perror("socket");
		parsed_flags_free(flags);
		exit(1);
	}

	struct hostent *hp; 	/*ptr to host info for remote*/
	struct sockaddr_in srv; /* used by connect() */

	hp = gethostbyname(flags->url->host);
	if(hp == NULL){
		perror("gethostbyname");
		parsed_flags_free(flags);
		close(fd);
		exit(1);
	}
	/* connect: use the Internet address family IPv4 */
	srv.sin_family = AF_INET;
	/* connect: socket ‘fd’ to port flags->url->port */
	int port = atoi(flags->url->port);
	srv.sin_port = htons(port);
	/* connect: connect to IP Address flags->url->host */
	srv.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;
	if(connect(fd, (struct sockaddr*) &srv, sizeof(srv)) < 0) {
		perror("connect");
		parsed_flags_free(flags);
		close(fd);
		exit(1);
	}
	char buffer[1024]; bzero(buffer, sizeof(buffer));
	int nbytes;
	if(flags->h)
		sprintf(buffer , "HEAD /%s HTTP/1.0\r\n" , flags->url->path);
	else
		sprintf(buffer , "GET /%s HTTP/1.0\r\n" , flags->url->path);
	if(flags->d){
		strcat(buffer , "If-Modified-Since: ");
		strcat(buffer , flags->time_interval);
		strcat(buffer , "\r\n\r\n");
	}
	else
		strcat(buffer , "\r\n");
	printf("HTTP request =\n%s\nLEN = %d\n", buffer, (int)strlen(buffer));
	if((nbytes = write(fd, buffer, sizeof(buffer))) < 0) {
		perror("write");
		parsed_flags_free(flags);
		close(fd);
		exit(1);
	}
	bzero(buffer, sizeof(buffer)); int totalBytes = 0;
	while((nbytes = read(fd, buffer, sizeof(buffer))) > 0){
		printf("%s", buffer);
		totalBytes += nbytes;
		bzero(buffer, sizeof(buffer));
	}
	if(nbytes < 0){
		perror("read");
		parsed_flags_free(flags);
		close(fd);
		exit(1);
	}
	printf("\nTotal received response bytes: %d\n",totalBytes);
	close(fd);
	parsed_flags_free(flags);
}

/*
 * the function searching for flags in the user input and flagged it.
 * also the function calling the parse_url function to parse the URL input.
 */
flags_t* parseFlags(int length , char** array)
{
	if(length < 2 || length >5 )
	{
		printf("Usage: client [-h] [-d <time-interval>] <URL>\n");
		exit(1);
	}
	flags_t *flags = (flags_t*)malloc(sizeof(flags_t));
	if(flags == NULL){
		perror("malloc");
		exit(1);
	}
	flags->d = flags->h = flags->urlFlag = FALSE;
	flags->time_interval = NULL;
	flags->url = NULL;
	int i;
	for(i = 1; i < length; i++){
		switch(array[i][0]){
			case '-' :
				if(!strcmp(array[i], "-h") && !flags->h)
					flags->h = TRUE;
				else if(!strcmp(array[i], "-d") && !flags->d){
					flags->d = TRUE;
					if(i+1 == length){
						printf("Usage: client [-h] [-d <time-interval>] <URL>\n");
						parsed_flags_free(flags);
						exit(1);
					}
					flags->time_interval = get_time_interval(array[i+1]);
					if(flags->time_interval == NULL){
						printf("Wrong input\n");
						parsed_flags_free(flags);
						exit(1);
					}
					i++;
				}
				else {
					printf("Usage: client [-h] [-d <time-interval>] <URL>\n");
					parsed_flags_free(flags);
					exit(1);
				}
				break;

			default:
				if(flags->urlFlag) {
					printf("Usage: client [-h] [-d <time-interval>] <URL>\n");
					parsed_flags_free(flags);
					exit(1);
				}
				flags->urlFlag = TRUE;
				flags->url = parse_url(array[i]);
				if(flags->url == NULL) {
					parsed_flags_free(flags);
					exit(1);
				}
				break;
		}
	}
	if(!flags->urlFlag) {
		printf("Usage: client [-h] [-d <time-interval>] <URL>\n");
		parsed_flags_free(flags);
		exit(1);
	}
	return flags;
}
/*
 * the function get time interval in format dd:hh:mm
 * and return time interval in format day, num ex. Sat, 17
 */
char* get_time_interval(char *timeinterval)
{
	char *day, *hour, *min;
	day = strtok(timeinterval , ":");
	hour = strtok(NULL , ":");
	min = strtok(NULL , ":");
	if(day == NULL || hour == NULL || min == NULL ||
	!strlen(day) || !strlen(hour) || !strlen(min) )
		return NULL;
	int i;
	for(i = 0; i < strlen(day); i++)
		if(!isdigit(day[i]))
			return NULL;
	for(i = 0; i < strlen(hour); i++)
		if(!isdigit(hour[i]))
			return NULL;
	for(i = 0; i < strlen(min); i++)
		if(!isdigit(min[i]))
			return NULL;
	int iday , ihour , imin;
	iday = atoi(day); ihour = atoi(hour); imin = atoi(min);
	time_t now;
	char *timebuf = (char*)malloc(sizeof(char)*128);
	bzero(timebuf , sizeof(timebuf));
	now = time(NULL);
	now = now-(iday*24*3600+ihour*3600+imin*60); //where day, hour and min are the values
	//from the input
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	//timebuf holds the correct format of the time
	return timebuf;
}


url_t* parse_url(char *url)
{
	char *p = strstr(url , "http://");
	if(p == NULL || p != url) {
		printf("Wrong input\n");
		return NULL;
	}
	url_t *purl;
	const char *tmpstr;
    const char *curstr;
    int len;

    /* Allocate the parsed url storage */
    purl = (url_t*)malloc(sizeof(url_t));
    if ( NULL == purl ) {
    	perror("malloc");
    	return NULL;
    }
    purl->host = purl->path = purl->port = NULL;

    curstr = url+7; // ignore the http://

    /* Proceed on by delimiters with reading host */
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) {
        if ( ':' == *tmpstr || '/' == *tmpstr ) {
            /* Port number or Path is specified. */
            break;
        }
        tmpstr++;
    }
    len = tmpstr - curstr;
    purl->host = (char*)malloc(sizeof(char) * (len + 1));
    if ( NULL == purl->host || len <= 0 ) {
    	if(len <= 0){
    		printf("Wrong input\n");
    		parsed_url_free(purl);
    	}
    	else{
    		perror("malloc");
    		parsed_url_free(purl);
    	}
        return NULL;
    }
    (void)strncpy(purl->host, curstr, len);
    purl->host[len] = '\0';
    curstr = tmpstr;

    /* Is port number specified? */
    if ( ':' == *curstr ) {
    	curstr++;
        /* Read port number */
    	tmpstr = curstr;
        while ( '\0' != *tmpstr && '/' != *tmpstr ) {
            if(!isdigit(*tmpstr)){
            	/* Port include characters. */
            	printf("Wrong input\n");
            	parsed_url_free(purl);
            	return NULL;
            }
        	tmpstr++;
        }
        len = tmpstr - curstr;
        if(len <= 0){
        	printf("Wrong input\n");
        	parsed_url_free(purl);
        	return NULL;
        }
        purl->port = (char*)malloc(sizeof(char) * (len + 1));
		if ( NULL == purl->port ) {
			perror("malloc");
			parsed_url_free(purl);
			return NULL;
		}
		(void)strncpy(purl->port, curstr, len);
    }
    else{
    	len = 2;
        purl->port = (char*)malloc(sizeof(char) * (len + 1));
        if ( NULL == purl->port ) {
        	perror("malloc");
        	parsed_url_free(purl);
    		return NULL;
        }
        (void)strncpy(purl->port, "80", len);
    }

    purl->port[len] = '\0';
    curstr = tmpstr;

    /* End of the string */
    if ( '\0' == *curstr ) {
    	len = 0;
        purl->path = (char*)malloc(sizeof(char) * (len + 1));
        if ( NULL == purl->path ) {
        	perror("malloc");
        	parsed_url_free(purl);
        	return NULL;
	   }
	   purl->path[len] = '\0';
	   return purl;
    }

    /* Skip '/' */
    if ( '/' != *curstr ) {
    	printf("Wrong input\n");
    	parsed_url_free(purl);
    	return NULL;
    }
    curstr++;

    /* Parse path */
    tmpstr = curstr;
    while ( '\0' != *tmpstr && '#' != *tmpstr  && '?' != *tmpstr ) {
        tmpstr++;
    }
    len = tmpstr - curstr;
    purl->path = (char*)malloc(sizeof(char) * (len + 1));
    if ( NULL == purl->path ) {
    	perror("malloc");
    	parsed_url_free(purl);
    	return NULL;
    }
    (void)strncpy(purl->path, curstr, len);
    purl->path[len] = '\0';
    curstr = tmpstr;
    return purl;
}

/*
 * Free memory of parsed url
 */
void parsed_url_free(url_t *purl)
{
    if ( NULL != purl ) {
        if ( NULL != purl->host ) {
            free(purl->host);
        }
        if ( NULL != purl->port ) {
            free(purl->port);
        }
        if ( NULL != purl->path ) {
            free(purl->path);
        }
        free(purl);
    }
}
/*
 * Free memory of flags
 */
void parsed_flags_free(flags_t *flags)
{
    if ( NULL != flags ) {
    	parsed_url_free(flags->url);
    	if ( NULL != flags->time_interval ) {
    	     free(flags->time_interval);
    	}
    	free(flags);
    }
}
