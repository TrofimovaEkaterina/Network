#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <cstdlib>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "service.h"


#include <sys/stat.h>


int send_to_the_death(int serv_sock, char* buf, int n_to_send);

int main(int argc, char **argv) {

    int serv_sock, portno, ret_snd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char* fn_length_buf;

    FILE* fptr;

    /***********************************************************/
    /* Check for correct arguments number                      */
    /***********************************************************/
    if (argc < 4) {
        fprintf(stderr, "usage %s hostname port filename\n", argv[0]);
        exit(TOO_FEW_ARG);
    }

    /***********************************************************/
    /* Get hostent structure to connect the server             */
    /***********************************************************/
    server = gethostbyname(argv[1]);

    if (server == NULL) {
        herror("gethostbyname() failed");
        fprintf(stderr, "Terminating...\n");
        exit(SERV_ADDR_ERR);
    }

    portno = atoi(argv[2]);


    /***********************************************************/
    /* Check if file for transfer exsists and access for       */
    /* reading                                                 */
    /***********************************************************/
    if (access(argv[3], R_OK) != 0) {
        fprintf(stdout, "file with name %s not exsists or there is no permission to read it. Transmission cancelation...\n", argv[3]);
        exit(ERR_FILE_ACCESS);
    }

    /***********************************************************/
    /* Open the file                                           */
    /***********************************************************/
    if ((fptr = fopen(argv[3], "rb")) == NULL) {
        error("fopen() failed");
    }

    struct stat st;
    if (stat(argv[3], &st) < 0) {
        error("stat() failed");
    }

    long int notWrited = st.st_size;
    printf("%ld\n", notWrited);

    /*************************************************************/
    /* Create an AF_INET stream socket to connect to server      */
    /*************************************************************/
    serv_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (serv_sock < 0)
        error("socket() failed");

    //зануляем структуру адреса
    bzero((char *) &serv_addr, sizeof (serv_addr));

    //вносим адрес хоста
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    /*************************************************************/
    /* Client trys to connect server                             */
    /*************************************************************/
    if (connect(serv_sock, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) < 0)
        error("connect() failed");

    /*************************************************************/
    /* Make msg with file_name_length                            */
    /*************************************************************/
    int fn_length = strlen(argv[3]);
    fn_length_buf = (char*) calloc(5, sizeof (char));

    sprintf(fn_length_buf, "%i\0", fn_length);

    /*************************************************************/
    /* File name length sending                                  */
    /*************************************************************/
    ret_snd = send_to_the_death(serv_sock, fn_length_buf, 4 * sizeof (char));
    printf("%d\n", ret_snd);

    if (ret_snd == SEND_ERR) {
        perror("send() failed");
        close(serv_sock);
        return errno;
    }

    /*************************************************************/
    /* File name sending                                         */
    /*************************************************************/
    ret_snd = send_to_the_death(serv_sock, argv[3], fn_length);
    printf("%s\n", argv[3]);

    if (ret_snd == SEND_ERR) {
        perror("send() failed");
        close(serv_sock);
        return errno;
    }

    char* buf = (char*) calloc(BUFSIZE + 1, sizeof (char));

    /*************************************************************/
    /* File content sending                                      */
    /*************************************************************/
    while (notWrited > 0) {

        int ret_frd;

        ret_frd = fread(buf, 1, BUFSIZE, fptr);
        printf("%d\n", ret_frd);

        if (ret_frd < 0) {
            error("fread() failed");
        }

        ret_snd = send_to_the_death(serv_sock, buf, ret_frd);
        printf("%d\n", ret_snd);

        if (ret_snd == SEND_ERR) {
            perror("send() failed");
            close(serv_sock);
            return errno;
        }

        notWrited -= ret_frd;

    }

    printf("File has been successfuly sended\n");

    close(serv_sock);
    return 0;
}

int send_to_the_death(int serv_sock, char* buf, int n_to_send) {

    int ret;
    int offset = 0;

    do {

        ret = send(serv_sock, &buf[offset], n_to_send, 0);
        printf("%d\n", ret);

        if (ret < 0) {
            return SEND_ERR;
        }

        n_to_send -= ret;

    } while (n_to_send > 0);

    return SUCCESS;
}