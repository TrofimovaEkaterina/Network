#ifndef OPT_H
#define OPT_H

#define DEF_PORT 14204
#define FD_MAX_COUNT 10
#define MSG_SIZE 256

void error(const char *msg, int code) {
    perror(msg);
    exit(code);
}


#endif /* OPT_H */

