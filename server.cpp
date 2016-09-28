#include <cstdlib>
#include <stdio.h> 
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "opt.h"

int main(int argc, char** argv) {

    int portno;

    if (argc < 2) {
        fprintf(stderr, "No port provided. The default one will be used: %d\n", DEF_PORT);
        portno = DEF_PORT;
    } else {
        portno = atoi(argv[1]);
    }

    int listen_sock = socket(AF_INET, SOCK_STREAM /*| SOCK_NONBLOCK*/, 0);

    if (listen_sock < 0) {
        error("Error on socket: ", 1);
    }

    struct sockaddr_in s_addr;

    bzero((char *) &s_addr, sizeof (s_addr));

    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(portno);

    bind(listen_sock, (struct sockaddr*) &s_addr, sizeof (s_addr));

    listen(listen_sock, 5);

    printf("Server started\nWaiting for incoming connection...\n");
    //fflush(stdout);

    //create a fd_set for accepting clients connections
    fd_set set;
    FD_ZERO(&set);

    /*
    int serial = 0;
    int fd_buf[FD_MAX_COUNT] = {0};
     */

    int client_sock = accept(listen_sock, NULL, NULL);
    FD_SET(client_sock, &set);


    if (client_sock < 0) {
        error("Error on accept: ", 2);
    }

    /*if (client_sock > 0 && serial < FD_MAX_COUNT) {
        FD_SET(client_sock, &set);
        fd_buf[serial] = client_sock;
        serial++;
    }
     */

    char* buf = (char*) calloc(MSG_SIZE, sizeof (char));

    struct timeval sel_tv;
    sel_tv.tv_sec = 10;
    sel_tv.tv_usec = 0;


    struct timeval start,
            start_of_circle,
            finish;


    gettimeofday(&start, NULL);
    start_of_circle = start;

    ulong byte_num = 0;
    ulong byte_num_for_circle = 0;

    double diff, globe_diff;

    fprintf(stderr, "select near\n");

    while (select(20000, &set, NULL, NULL, &sel_tv) > 0) {
        byte_num_for_circle += recv(client_sock, &buf, 256, 0);

        gettimeofday(&finish, NULL);

        diff = (double) ((finish.tv_sec - start_of_circle.tv_sec)*1000 + (finish.tv_usec - start_of_circle.tv_usec) / 1000.0);

        //timeout renovation
        sel_tv.tv_sec = 1;
        sel_tv.tv_usec = 0;

        //check for a second passed
        if (diff >= 1000.0) {
            globe_diff = (double) ((finish.tv_sec - start.tv_sec)*1000 + (finish.tv_usec - start.tv_usec) / 1000.0);
            byte_num += byte_num_for_circle;
            fprintf(stderr, "Speed: %u b/s; Avg: %u b/s;\n", (uint) (byte_num_for_circle / (diff / 1000)), (uint) (byte_num / (globe_diff / 1000)));
            fflush(stdout);
            byte_num_for_circle = 0;
            gettimeofday(&start_of_circle, NULL);
        }

    }

    close(client_sock);
    close(listen_sock);

    return 0;
}

