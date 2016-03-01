#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct parsed_url {
    char *host;                 /* mandatory */
    int port;                 /* optional */
    char *path;                 /* optional */
};

struct parsed_url * parse_url(char *);
void parsed_url_free(struct parsed_url *);

  int main(int argc, char **argv) {
	  struct parsed_url *url = parse_url("http://www.google.com/search");
	  printf("%s\n" , url->host);

	  printf("%s\n" , url->path);
	  printf("%s\n" , url->port);
}

/*
 * See RFC 1738, 3986
 */
struct parsed_url *
parse_url(char *url)
{
    struct parsed_url *purl;
    const char *tmpstr;
    const char *curstr;
    int len;
    int i;

    /* Allocate the parsed url storage */
    purl = malloc(sizeof(struct parsed_url));
    if ( NULL == purl ) {
        return NULL;
    }
    purl->host = purl->port = purl->path = NULL;

    curstr = url+7;

    /* Proceed on by delimiters with reading host */
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) {
        if ( ':' == *tmpstr || '/' == *tmpstr ) {
            /* Port number is specified. */
            break;
        }
        tmpstr++;
    }
    len = tmpstr - curstr;
    purl->host = malloc(sizeof(char) * (len + 1));
    if ( NULL == purl->host || len <= 0 ) {
        parsed_url_free(purl);
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
            	parsed_url_free(purl);
            	return NULL;
            }
        	tmpstr++;
        }
        len = tmpstr - curstr;
        purl->port = malloc(sizeof(char) * (len + 1));
        if ( NULL == purl->port ) {
            parsed_url_free(purl);
            return NULL;
        }
        //(void)strncpy(purl->port, curstr, len);
        //purl->port[len] = '\0';
        purl->port = atoi(curstr);
        curstr = tmpstr;
    }

    /* End of the string */
    if ( '\0' == *curstr ) {
        return purl;
    }

    /* Skip '/' */
    if ( '/' != *curstr ) {
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
    purl->path = malloc(sizeof(char) * (len + 1));
    if ( NULL == purl->path ) {
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
void
parsed_url_free(struct parsed_url *purl)
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
