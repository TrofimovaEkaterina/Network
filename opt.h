#ifndef OPT_H
#define OPT_H

#define DEF_SEND_BC_PORT 14204
#define DEF_LIST_BC_PORT 14203
#define MSG_SIZE 17          
#include <unistd.h>
#include <stdlib.h>

void error(const char *msg, int code) {
    perror(msg);
    exit(code);
}


#endif /* OPT_H */

