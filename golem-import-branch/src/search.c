/* search.c - perform searches for golem */

/* #includes for sockets */
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

/* Other #includes */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "search.h"
#include "loew.h"

/* Connect to a given tcp stream socket */
int connect_socket(char *host_name, int port) {
  struct sockaddr_in serv_addr;
  char *host_address = "127.0.0.1";
  int sock_fd;
  struct protoent *pent;

  struct hostent *hent;
  char** hadd;
  struct in_addr in;

  /* Get host address */
  hent = gethostbyname(host_name);

  if (hent == NULL) {
    printf ("Host \"%s\" cannot be located\n", host_name);
    exit (1);
  }

  hadd = hent->h_addr_list;
  memcpy(&in.s_addr, *hadd, sizeof(in.s_addr));
  host_address = inet_ntoa(in);

  /* Get protocol details */
  pent = getprotobyname("tcp");
  if(pent == NULL) {
    fprintf(stderr, "getprotobyname() failed\n");
    return -1;
  }

  /* Setup socket address */
  memset (&serv_addr, 0, sizeof (serv_addr));
  serv_addr.sin_family        = AF_INET;
  serv_addr.sin_port          = htons (port);
  serv_addr.sin_addr.s_addr   = in.s_addr;

  /* Create socket */
  if((sock_fd = socket(AF_INET, SOCK_STREAM, pent->p_proto)) < 0) {
    fprintf(stderr, "Error opening socket.\n");
    return -1;
  }

  /* Connect to server */
  if(connect(sock_fd, (struct sockaddr*) &serv_addr, sizeof (serv_addr)) < 0) {
    fprintf(stderr, "Error in connecting to %s (%s) port %d\n",
            host_name, host_address, port);
    return -1;
  }

  return(sock_fd);
}

int is_search_char(char p) {
  if(p >= 'A' && p <= 'Z') return 1;
  if(p >= 'a' && p <= 'z') return 1;
  if(p >= '0' && p <= '9') return 1;
  if(p == '-') return 1;
  return 0;
}

char **search_mono(char *query) {
  char **results;
  int www_fd;
  char *p, *q;
  ssize_t bytes;
  char tmp[MAXSTR];
  char tmp2[MAXSTR];
  char *p2;
  int count;
  int state;
  int state2;

  results = malloc(sizeof(char *) * MAXRESULTS);
    
  www_fd = connect_socket("localhost", 80);
  if(www_fd < 0) {
    results[0] = strdup("I'm sorry - the index of mono is currently unavailable");
    results[1] = NULL;
    return results;
  }

  strcpy(tmp, "GET /cgi-bin/fx-1.4?DB=mono2&P=");
  q = tmp + strlen(tmp);
  for(p = query; *p != '\0'; p++) {
    if(is_search_char(*p)) *q++ = *p;
    if(*p <= ' ') *q++ = '+';
  }
  *q++ = '\n';
  *q++ = '\0';
  fprintf(stderr, "Sending query request: `%s'", tmp);
  write(www_fd, tmp, strlen(tmp));

  count = 0;
  state = 0;
  state2 = 0;
  p2 = tmp2;
  while(count < MAXRESULTS - 1) {
    bytes = read(www_fd, tmp, MAXSTR - 1);
    if(bytes == 0) break;
    tmp[bytes] = '\0';

    for(p = tmp; *p != '\0'; p++) {
      if(*p < 32 || *p > 126) *p = ' ';

      if(*p == '<') state2 = 1;
      else if(*p == '/' && state2 == 1) ;
      else if(*p == 'T' && state2 == 1) state2 = 2;
      else if(*p == 'B' && state2 == 1) state2 = 3;
      else if(*p == 'R' && state2 == 3) {
	state2 = 0;
	state = 100;
      }
      else if(*p == 'R' && state2 == 2) {
        state2 = 0;
	state = 1;

	if(p2 != tmp2) {
	  *p2 = '\0';
	  fprintf(stderr, "Search results: `%s'\n", tmp2);
	  results[count++] = strdup(tmp2);
	  p2 = tmp2;
	}
      }
      else if(*p == 'D' && state2 == 2) state2 = 4;
      else if(state2 == 4) {
	if(*p == '>') {
          state2 = 5;
	  state += 1;
	}
      }
      else if(state2 == 5) {
	state2 = 0;
      } else {
	state2 = 0;
      }

      if(state == 6) {
	if(state2 == 0) {
	  *p2++ = *p;
        }
      }
    }
  }
  results[count] = NULL;

  return results;
}

char **search_run(char *index, char *query) {
  char **results;

  if(!strcmp(index, "mono")) {
    results = search_mono(query);
  } else {
    results = malloc(sizeof(char *) * 2);
    results[0] = malloc(sizeof(char) * MAXSTR);
    snprintf(results[0], MAXSTR - 1,
	     "Index `%s' not found - Unable to run search `%s'",
	     index, query);
    results[0][MAXSTR - 1] = '\0';
    results[1] = NULL;
  }

  return results;
}
