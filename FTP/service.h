#ifndef OPT_H
#define OPT_H

#define DEF_PORT 14204
#define BUFSIZE 256 
#define MAX_FULL_NAME_LENGTH 200
#define MAX_CONN_N 10   /*max number of connections*/

#define SEND_ERR -6
#define ERR_FILE_ACCESS -5
#define SERV_ADDR_ERR -4
#define TOO_FEW_ARG -3
#define FILE_WR_ERR -2
#define SUCCESS 0
#define CONN_CLOSED_BY_CLI 1
#define INVALID_NAME_LENGTH 2
#define MEM_ALLOC_FAIL 3

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

void error(const char *msg) {
    perror(msg);
    exit(errno);
}


#endif /* OPT_H */

