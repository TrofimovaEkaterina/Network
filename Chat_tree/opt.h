#ifndef OPT_H
#define OPT_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <vector>
#include <ctime>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <malloc.h>
#include <signal.h>

#define MSG (char)0
#define CONNECT_REQUEST (char)1
#define CONNECT_TO (char)2
#define DISCONNECT (char)3
#define ACK (char)4

#define MSG_SIZE 200
#define MSG_ID_RANGE 1000000
#define UNSENT_MSGS_LIMIT 5
#define DEFAULT_ATTEMPT_NUMB 3

#define TIMEOUT 2
#define MSG_QUEUE_MAX_SIZE 40

typedef struct send_info {
    uint msg_id;
    uint attempt;
    time_t last_attempt;
} send_info;

typedef struct node {
    struct sockaddr_in addr;
    std::vector<send_info> msgs_to_send;
    bool is_parent;
    uint death_counter;
} node;

typedef struct message {
    uint id;
    char * package; /*--По сути готовое к отправке сообщение с вложенным типом сообщения и его id*/
    int send_counter;
} message;


#define SIO sizeof (uint) + sizeof (char)

#endif /* OPT_H */

