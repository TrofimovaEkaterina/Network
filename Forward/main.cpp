#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include <string.h>
#include <string>

#include <unistd.h>
#include <errno.h>
#include <malloc.h>

#include <time.h>

#include <netdb.h>
#include <list>
#include <stdlib.h>

enum STATE {
    CONNECT, FORWARD
};

using namespace std;

fd_set read_FD, write_FD;
#define BUFSIZE 1024
#define FAILED -1;
#define SUCCESS 0;
#define TIMEOUT 20

typedef struct ser_cli {
    int serv_FD;
    int cli_FD;

    char * buf;
    int buf_size;
    int avail_data_count;
    long int offset;

    char * _buf;
    int _buf_size;
    int _avail_data_count;
    long int _offset;

    STATE state;

    time_t last_appeal;
} ser_cli;

std::list<ser_cli> pairs;

void clean_pair(list<ser_cli>::iterator pair);
int forward(list<ser_cli>::iterator pair);
int accept(int listen_sock_FD);
int con(list<ser_cli>::iterator pair);


struct sockaddr_in addr;

int hostport;
char* hostname;

int main(int argc, char** argv) {

    int listen_sock_FD;
    int ret;
    int lport;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <listen port> <host name> <host port> \n", argv[0]);
        exit(0);
    } else {
        lport = atoi(argv[1]);
        hostport = atoi(argv[3]);

        if (lport == 0 || hostport == 0) {
            fprintf(stderr, "Invalid port number\n");
            exit(0);
        }

        hostname = (char*) calloc(1, strlen(argv[2]) + 1);
        memcpy(hostname, argv[2], strlen(argv[2]));
    }

    listen_sock_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_FD < 0) {
        perror("socket() failed");
        exit(0);
    }

    bzero((char *) &addr, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(lport);

    int on = 1;
    ret = ioctl(listen_sock_FD, FIONBIO, (char *) &on);
    if (ret < 0) {
        perror("ioctl() failed");
        close(listen_sock_FD);
        exit(0);
    }

    ret = setsockopt(listen_sock_FD, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof (on));

    if (ret < 0) {
        perror("setsockopt() failed");
        close(listen_sock_FD);
        exit(0);
    }

    ret = bind(listen_sock_FD, (struct sockaddr *) &addr, sizeof (addr));
    if (ret < 0) {
        perror("bind() failed");
        close(listen_sock_FD);
        exit(0);
    }

    ret = listen(listen_sock_FD, 100);

    if (ret < 0) {
        perror("listen() failed");
        close(listen_sock_FD);
        exit(0);
    }

    printf("Waiting for incoming connection...\n");

    bzero((char *) &addr, sizeof (addr));

    struct hostent *he;

    if ((he = gethostbyname(hostname)) == NULL) {
        herror("gethostbyname");
        exit(0);
    }


    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((in_addr*) he->h_addr_list[0])->s_addr;
    addr.sin_port = htons(hostport);


    FD_ZERO(&read_FD);
    FD_ZERO(&write_FD);
    struct timeval sel_time;
    int nfds, sel_ret;


    /*Forward*/
    while (true) {

        sel_time.tv_sec = 5;
        sel_time.tv_usec = 0;

        FD_ZERO(&read_FD);
        FD_ZERO(&write_FD);

        FD_SET(listen_sock_FD, &read_FD);
        nfds = listen_sock_FD + 1;

        for (std::list<ser_cli>::iterator pair = pairs.begin(); pair != pairs.end(); pair++) {

            /*Не обязательная опция!*/
            /*Внутренний таймаут - чтобы не ждать дисконекта сервера или клиента при продолжительном простое линии*/
            if (difftime(time(NULL), pair->last_appeal) > TIMEOUT) {
                fprintf(stderr, "Time is out\n");
                clean_pair(pair);
            }


            if (pair->cli_FD != 0) {

                if (pair->cli_FD > nfds) nfds = pair->cli_FD;

                /*Если есть свободное место в буфере...*/
                if (pair->buf_size - pair->avail_data_count > 0)
                    FD_SET(pair->cli_FD, &read_FD);
                /*Если есть что читать из буфера...*/
                if (pair->_avail_data_count - pair->_offset > 0)
                    FD_SET(pair->cli_FD, &write_FD);
            }


            if (pair->serv_FD != 0) {

                if (pair->serv_FD > nfds) nfds = pair->serv_FD;

                /*Если есть свободное место в буфере...*/
                if (pair->_buf_size - pair->_avail_data_count > 0)
                    FD_SET(pair->serv_FD, &read_FD);
                /*Если есть что читать из буфера...*/
                if (pair->avail_data_count - pair->offset > 0) {
                    FD_SET(pair->serv_FD, &write_FD);
                }
            }


            if (pair->cli_FD == pair->serv_FD) {
                pair = pairs.erase(pair);
            }

        }

        nfds++;


        sel_ret = select(nfds, &read_FD, &write_FD, NULL, &sel_time);

        if (sel_ret == 0) {
            continue;
        }

        if (sel_ret < 0) {
            perror("\nselect() failed");
            exit(0);
        }

        if (FD_ISSET(listen_sock_FD, &read_FD)) {
            accept(listen_sock_FD);
        }

        for (std::list<ser_cli>::iterator pair = pairs.begin(); pair != pairs.end(); pair++) {

            if (pair->state == CONNECT) {
                con(pair);
            }

            if (pair->state == FORWARD) {
                forward(pair);
            }
        }

    }


    fprintf(stderr, "Forward done\n");

    return 0;
}

int forward(list<ser_cli>::iterator pair) {

    //from buf_ to cli
    if (FD_ISSET(pair->cli_FD, &write_FD)) {

        pair->last_appeal = time(NULL);

        fprintf(stderr, "from _buf to cli %d\n", pair->cli_FD);

        int ret = send(pair->cli_FD, (pair->_buf + pair->_offset), (pair->_avail_data_count - pair->_offset), MSG_NOSIGNAL);

        if (ret < 0) {
            if (errno != EWOULDBLOCK) {
                perror("send() failed 1");
                fprintf(stderr, "Connection would be closed...\n\n");
                clean_pair(pair);
                return FAILED;
            }
        }


        pair->_offset += ret;

        /*Если к этому моменту сервер закрыл соединение, а клиент считал все, что было в буфере - закрываем коннект*/
        if (pair->_avail_data_count == pair->_offset && pair->serv_FD == 0) {
            fprintf(stderr, "Client got all available data from server %s\n", hostname);
            clean_pair(pair);
            return SUCCESS;
        }

        /*Если клиент закрыл соединение*/
        if (ret == 0) {
            fprintf(stderr, "Connection closed by cli\n");

            close(pair->cli_FD);
            pair->cli_FD = 0;

            /*Если при этом сервер считал все данные из буфера, то закрываем полностью коннект*/
            if (pair->avail_data_count == pair->offset) {
                clean_pair(pair);
                return SUCCESS;
            }
        }

        if (ret > 0) {
            /*if we full fill the buffer and send all its content to client...*/
            if (pair->_avail_data_count == pair->_offset && pair->_offset == pair->_buf_size) {
                pair->_avail_data_count = 0;
                pair->_offset = 0;
            }
        }
    }

    //from buf to serv
    if (FD_ISSET(pair->serv_FD, &write_FD)) {

        fprintf(stderr, "from buf to serv %d\n", pair->serv_FD);

        pair->last_appeal = time(NULL);

        int ret = send(pair->serv_FD, (pair->buf + pair->offset), (pair->avail_data_count - pair->offset), MSG_NOSIGNAL);

        if (ret < 0) {
            if (errno != EWOULDBLOCK) {
                perror("send() failed 2");
                fprintf(stderr, "Connection would be closed...\n\n");
                clean_pair(pair);
                return FAILED;
            }
        }


        pair->offset += ret;

        /*Если к этому моменту клиент закрыл соединение, а сервер считал все, что было в буфере - закрываем коннект*/
        if (pair->avail_data_count == pair->offset && pair->cli_FD == 0) {
            fprintf(stderr, "%s got all available data from cli\n\n", hostname);
            clean_pair(pair);
            return SUCCESS;
        }

        /*Если сервер закрыл соединение*/
        if (ret == 0) {
            fprintf(stderr, "Connection closed by serv %s\n", hostname);

            close(pair->serv_FD);
            pair->serv_FD = 0;

            /*Если при этом клиент считал все данные из буфера, то закрываем полностью коннект*/
            if (pair->_avail_data_count == pair->_offset) {
                clean_pair(pair);
                return SUCCESS;
            }
        }

        if (ret > 0) {
            /*if client full fill the buffer and send all its content to server...*/
            if (pair->offset == pair->buf_size) {
                pair->avail_data_count = 0;
                pair->offset = 0;
            }
        }
    }


    //from serv to _buf
    if (FD_ISSET(pair->serv_FD, &read_FD)) {

        fprintf(stderr, "from serv %d to _buf\n", pair->serv_FD);

        pair->last_appeal = time(NULL);

        /*вдруг к этому моменту клиент отвалился - остается только дочитать данные из др буфера если нужно*/
        if (pair->cli_FD == 0) {
            /*заглушка - чтобы сервер больше не писал в буфер*/
            pair->_avail_data_count = pair->_buf_size;
            return FAILED;
        }

        int ret = recv(pair->serv_FD, (pair->_buf + pair->_avail_data_count), (pair->_buf_size - pair->_avail_data_count), MSG_NOSIGNAL);

        if (ret < 0) {
            if (errno != EWOULDBLOCK) {
                perror("recv() failed 3");
                fprintf(stderr, "Connection would be closed... %d\n\n", errno);
                clean_pair(pair);
                return FAILED;
            }
        }

        /*Сервер закрыл соединение*/
        if (ret == 0) {
            fprintf(stderr, "Connection closed by serv %s\n", hostname);

            close(pair->serv_FD);
            pair->serv_FD = 0;

            /*Если при этом клиент считал все данные из буфера, то закрываем полностью коннект*/
            if (pair->_avail_data_count == pair->_offset) {
                clean_pair(pair);
                return SUCCESS;
            }
        }

        if (ret > 0) {
            pair->_avail_data_count += ret;
        }

    }



    //from client to buf
    if (FD_ISSET(pair->cli_FD, &read_FD)) {

        fprintf(stderr, "from cli %d to buf\n", pair->cli_FD);

        pair->last_appeal = time(NULL);

        /*вдруг к этому моменту сервер отвалился - остается только дочитать данные из др буфера если нужно*/
        if (pair->serv_FD == 0) {
            /*заглушка - чтобы клиент больше не писал в буфер*/
            pair->avail_data_count = pair->buf_size;
            return FAILED;
        }

        int ret = recv(pair->cli_FD, (pair->buf + pair->avail_data_count), (pair->buf_size - pair->avail_data_count), MSG_NOSIGNAL);

        if (ret < 0) {
            if (errno != EWOULDBLOCK) {
                perror("recv() failed 4");
                fprintf(stderr, "Connection would be closed... %d\n\n", errno);
                clean_pair(pair);
                return FAILED;
            }
        }

        /*Клиент закрыл соединение*/
        if (ret == 0) {
            fprintf(stderr, "Connection closed by cli\n");

            close(pair->cli_FD);
            pair->cli_FD = 0;

            /*Если при этом сервер считал все данные из буфера, то закрываем полностью коннект*/
            if (pair->avail_data_count == pair->offset) {
                clean_pair(pair);
                return SUCCESS;
            }
        }

        if (ret > 0) {
            pair->avail_data_count += ret;
        }

    }

    return SUCCESS;

}

/*Очистка на случай форс мажора*/
void clean_pair(std::list<ser_cli>::iterator pair) {

    if (pair->serv_FD != 0) {
        fprintf(stderr, "Close connection with server\n");
        shutdown(pair->serv_FD, SHUT_RDWR);
        close(pair->serv_FD);
        pair->serv_FD = 0;
    }

    if (pair->cli_FD != 0) {
        fprintf(stderr, "Close connection with client\n");
        shutdown(pair->cli_FD, SHUT_RDWR);
        close(pair->cli_FD);
        pair->cli_FD = 0;
    }

    if (pair->buf) {
        free(pair->buf);
    }

    if (pair->_buf) {
        free(pair->_buf);
    }

    fprintf(stderr, "Connection closed\n\n");

}

int accept(int listen_sock_FD) {

    fprintf(stderr, "Incoming connection accepting...  ");

    ser_cli pair;

    pair.cli_FD = accept(listen_sock_FD, NULL, NULL);

    if (pair.cli_FD < 0) {
        fprintf(stderr, "failed\n");
        perror("accept()");
        return FAILED;
    }

    int on = 1;
    int ret = ioctl(pair.cli_FD, FIONBIO, (char *) &on);

    if (ret < 0) {
        fprintf(stderr, "failed\n");
        perror("ioctl()");
        return FAILED;
    }

    if ((pair.buf = (char*) calloc(1, BUFSIZE)) == NULL) {
        return FAILED;
    }
    if ((pair._buf = (char*) calloc(1, BUFSIZE)) == NULL) {
        free(pair.buf);
        return FAILED;
    }

    pair._buf_size = BUFSIZE;
    pair.buf_size = BUFSIZE;
    pair.offset = 0;
    pair._offset = 0;
    pair.avail_data_count = 0;
    pair._avail_data_count = 0;

    pair.last_appeal = time(NULL);
    pair.state = CONNECT;

    pairs.push_back(pair);



    fprintf(stderr, "success\n");

    fprintf(stderr, "pairs.size() %d\n", pairs.size());
    return SUCCESS;
}

int con(list<ser_cli>::iterator pair) {

    fprintf(stderr, "Creating connection to %s\n", hostname);

    pair->serv_FD = socket(AF_INET, SOCK_STREAM, 0);

    if (pair->serv_FD < 0) {
        fprintf(stderr, "failed\n");
        perror("socket()");
        clean_pair(pair);
        return FAILED;
    }

    int on = 1;
    int ret = ioctl(pair->serv_FD, FIONBIO, (char *) &on);

    if (ret < 0) {
        fprintf(stderr, "failed\n");
        perror("ioctl()");
        clean_pair(pair);
        return FAILED;
    }

    if (connect(pair->serv_FD, (struct sockaddr *) &addr, sizeof (addr)) < 0 && errno != EINPROGRESS) {
        fprintf(stderr, "failed\n");
        perror("connect()");
        clean_pair(pair);
        return FAILED;
    }

    pair->state = FORWARD;

    return SUCCESS;
}
