#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string>
#include <string.h>

#include "service.h"

struct transfer_info {
    FILE* fptr;
    char* full_file_name;
    char* short_file_name;
    int file_name_length;
};

int recv_file_name_length(struct transfer_info* tr_info, struct pollfd* pfd);
int recv_file_name(struct transfer_info* tr_info, struct pollfd* pfd);
int recv_file_cont(struct transfer_info* tr_info, struct pollfd* pfd);

main(int argc, char *argv[]) {
    int ret, on = 1, i, j;
    int listen_sock = -1, client_sock = -1;

    bool shut_down_server = false, compress_array = true;
    bool close_conn = false;


    struct sockaddr_in sock_addr;

    int timeout;
    struct pollfd fds[MAX_CONN_N];
    struct transfer_info tr_info[MAX_CONN_N];

    int nfds = 1, current_size = 0;

    int portno;


    /*************************************************************/
    /* Check for port provided                                   */
    /*************************************************************/
    if (argc < 2) {
        fprintf(stderr, "No port provided. The default one will be used: %d\n", DEF_PORT);
        portno = DEF_PORT;
    } else {
        portno = atoi(argv[1]);
    }


    /*************************************************************/
    /* Create an AF_INET stream socket to receive incoming       */
    /* connections on                                            */
    /*************************************************************/
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_sock < 0) {
        perror("socket() failed");
        exit(-1);
    }

    /*************************************************************/
    /* Allow socket descriptor to be reuseable                   */
    /*************************************************************/
    ret = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof (on));

    if (ret < 0) {
        perror("setsockopt() failed");
        close(listen_sock);
        exit(-1);
    }

    /*************************************************************/
    /* Set socket to be nonblocking. All of the sockets for    */
    /* the incoming connections will also be nonblocking since  */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    ret = ioctl(listen_sock, FIONBIO, (char *) &on);

    if (ret < 0) {
        perror("ioctl() failed");
        close(listen_sock);
        exit(-1);
    }

    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    memset(&sock_addr, 0, sizeof (sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_addr.sin_port = htons(portno);

    ret = bind(listen_sock, (struct sockaddr *) &sock_addr, sizeof (sock_addr));

    if (ret < 0) {
        perror("bind() failed");
        close(listen_sock);
        exit(-1);
    }

    /*************************************************************/
    /* Set the listen back log                                   */
    /*************************************************************/
    ret = listen(listen_sock, 32);

    if (ret < 0) {
        perror("listen() failed");
        close(listen_sock);
        exit(-1);
    }


    printf("Server started\n");


    /*************************************************************/
    /* Initialize the pollfd structure                           */
    /*************************************************************/
    memset(fds, 0, sizeof (fds));

    /*************************************************************/
    /* Set up the initial listening socket                        */
    /*************************************************************/
    fds[0].fd = listen_sock;
    fds[0].events = POLLIN;

    /*************************************************************/
    /* Initialize the timeout to 2 minutes.                      */
    /*************************************************************/
    timeout = (2 * 60 * 1000);


    /*************************************************************/
    /* Initialize the transfer_info structure                    */
    /*************************************************************/
    memset(tr_info, 0, sizeof (tr_info));

    /*************************************************************/
    /* Loop waiting for incoming connects or for incoming data   */
    /* on any of the connected sockets.                          */
    /*************************************************************/
    do {

        printf("Waiting on poll() for incoming connections or data...\n");
        ret = poll(fds, nfds, timeout);

        /***********************************************************/
        /* If the poll call failed.                                */
        /***********************************************************/
        if (ret < 0) {
            perror("  poll() failed");
            printf("  Server is going to shut down...\n");
            break;
        }

        /***********************************************************/
        /* If the time out expired.                       */
        /***********************************************************/
        if (ret == 0) {
            printf("  poll() timed out. Server is going to shut down...\n");
            break;
        }



        /*********************************************************/
        /* Loop through to find the descriptors that returned    */
        /* POLLIN and determine whether it's the listening       */
        /* or the active connection.                             */
        /*********************************************************/

        current_size = nfds; /*number of fds we are looking through*/

        for (i = 0; i < current_size; i++) {
            if (fds[i].revents == 0)
                continue;

            /*********************************************************/
            /* If revents is not POLLIN (an unexpected result)       */
            /* log and end shut down server.                         */
            /*********************************************************/
            if (fds[i].revents != POLLIN) {
                printf("  Error on some revents (%d)\n--Server is going to shut down...\n", fds[i].revents);
                shut_down_server = true;
                break;

            }
            if (fds[i].fd == listen_sock) {

                printf("    Listening socket is readable\n");

                /*******************************************************/
                /* Accept all incoming connections that are queued up  */
                /* on the listening socket. If accept fails with       */
                /* EWOULDBLOCK, then we have accepted all of them.     */
                /* Any other failure will cause us to end the server.  */
                /*******************************************************/
                do {

                    client_sock = accept(listen_sock, NULL, NULL);
                    if (client_sock < 0) {
                        if (errno != EWOULDBLOCK) {
                            perror("  accept() failed");
                            printf("  Server is going to shut down...\n");
                            shut_down_server = true;
                        }
                        break;
                    }

                    /*****************************************************/
                    /* Add the new incoming connection to the            */
                    /* pollfd structure                                  */
                    /*****************************************************/
                    printf("  New incoming connection: %d\n", client_sock);
                    fds[nfds].fd = client_sock;
                    fds[nfds].events = POLLIN;
                    tr_info[nfds].file_name_length = 0;
                    tr_info[nfds].full_file_name = NULL;
                    tr_info[nfds].fptr = NULL;
                    nfds++;

                } while (true);
            }                /*********************************************************/
                /* Not the listening readable socket                     */
                /*********************************************************/

            else {

                printf("    Descriptor %d is readable\n", fds[i].fd);
                close_conn = false;
                /*******************************************************/
                /* Receive all incoming data on this socket.           */
                /*******************************************************/

                do {
                    /*****************************************************/
                    /* Receive data on this connection until the         */
                    /* recv fails with EWOULDBLOCK. If any other         */
                    /* failure occurs, connection will be closed         */
                    /*****************************************************/

                    //ret = recv(fds[i].fd, buffer, sizeof (buffer), 0);

                    if (tr_info[i].file_name_length < 1) {
                        ret = recv_file_name_length(&tr_info[i], &fds[i]);

                        switch (ret) {
                            case SUCCESS:
                                printf("  File name length was successfully readed.\n");
                                break;
                            case EWOULDBLOCK:
                                printf("  Scip the connection - no data on recv()\n");
                                break;
                            case CONN_CLOSED_BY_CLI:
                                printf("  Connection has been closed by the client.\n  Connection is going to be closed from the server side...\n");
                                close_conn = true;
                                break;
                            case INVALID_NAME_LENGTH:
                                printf("  Received file name length is invalid.\n  Connection is going to be closed...\n");
                                close_conn = true;
                                break;
                            default:
                                perror("  recv() failed");
                                printf("  Connection is going to be closed...\n");
                                close_conn = true;
                                break;
                        }

                        /*****************************************************/
                        /* Need to break the loop if file name length        */
                        /* receiving wasn't successful                       */
                        /*****************************************************/
                        if (ret != SUCCESS) {
                            break;
                        }

                    }

                    if (tr_info[i].full_file_name == NULL) {
                        ret = recv_file_name(&tr_info[i], &fds[i]);

                        switch (ret) {
                            case SUCCESS:
                                printf("  File name was successfully readed:\n");
                                printf("  -Full name:  %s\n", tr_info[i].full_file_name);
                                printf("  -Short name: %s\n", tr_info[i].short_file_name);
                                break;
                            case EWOULDBLOCK:
                                printf("  Scip the connection - no data on recv()\n");
                                break;
                            case CONN_CLOSED_BY_CLI:
                                printf("  Connection has been closed by the client.\n  Connection is going to be closed from the server side...\n");
                                close_conn = true;
                                break;
                            case MEM_ALLOC_FAIL:
                                perror("  calloc() failed");
                                printf("  Connection is going to be closed...\n");
                                close_conn = true;
                                break;
                            default:
                                perror("  recv() failed");
                                printf("  Connection is going to be closed...\n");
                                close_conn = true;
                                break;
                        }

                        /*****************************************************/
                        /* Need to break the loop if file name receiving     */
                        /* wasn't successful                                 */
                        /*****************************************************/
                        if (ret != SUCCESS) {
                            break;
                        }

                    }

                    if (tr_info[i].fptr == NULL) {

                        if (access(tr_info[i].short_file_name, F_OK) == 0) {
                            printf("  File with name %s already exsists.\n Connection is going to be closed...\n", tr_info[i].short_file_name);
                            close_conn = true;
                            break;
                        }

                        if ((tr_info[i].fptr = fopen(tr_info[i].short_file_name, "wb")) == NULL) {
                            perror("  fopen() failed");
                            printf("  Connection is going to be closed...\n");
                            close_conn = true;
                            break;
                        }

                    }

                    ret = recv_file_cont(&tr_info[i], &fds[i]);
                    switch (ret) {
                        case SUCCESS:
                            printf("  File part content was successfully readed.\n");
                            break;
                        case EWOULDBLOCK:
                            printf("  Scip the connection - no data on recv()\n");
                            break;
                        case CONN_CLOSED_BY_CLI:
                            printf("  Connection has been closed by the client. File has been written.\n  Connection is going to be closed from the server side...\n");
                            close_conn = true;
                            break;
                        case FILE_WR_ERR:
                            perror("  fwrite() failed");
                            printf("  Connection is going to be closed...\n");
                            close_conn = true;
                            break;
                        default:
                            perror("  recv() failed");
                            printf("  Connection is going to be closed...\n");
                            close_conn = true;
                            break;
                    }

                    /*****************************************************/
                    /* Need to break the loop if file content            */
                    /* receiving wasn't successful                       */
                    /*****************************************************/
                    if (ret != SUCCESS) {
                        break;
                    }



                } while (true);

                /*******************************************************/
                /* If the close_conn flag was turned on, we need       */
                /* to clean up this active connection                  */
                /*******************************************************/
                if (close_conn) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    if (tr_info[i].full_file_name != NULL) {
                        free(tr_info[i].full_file_name);
                    }
                    if (tr_info[i].fptr != NULL) {
                        fclose(tr_info[i].fptr);
                    }

                    compress_array = true;
                }


            } /* End of existing connection is readable             */
        } /* End of loop through pollable descriptors              */

        /***********************************************************/
        /* If the compress_array flag was turned on, we need       */
        /* to do it and decrement the number of file descriptors.  */
        /***********************************************************/
        if (compress_array) {
            compress_array = false;
            for (i = 0; i < nfds; i++) {
                if (fds[i].fd == -1) {
                    for (j = i; j < nfds; j++) {
                        fds[j].fd = fds[j + 1].fd;
                        tr_info[j].file_name_length = tr_info[j + 1].file_name_length;
                        tr_info[j].full_file_name = tr_info[j + 1].full_file_name;
                        tr_info[j].short_file_name = tr_info[j + 1].short_file_name;
                    }
                    nfds--;
                }
            }
        }

    } while (!shut_down_server); /* End of serving running.    */

    /*************************************************************/
    /* Clean up all of the sockets that are open and all transfer*/
    /* info                                                      */
    /*************************************************************/
    for (i = 0; i < nfds; i++) {
        if (fds[i].fd >= 0)
            close(fds[i].fd);
        if (tr_info[i].full_file_name != NULL) {
            free(tr_info[i].full_file_name);
        }
        if (tr_info[i].fptr != NULL) {
            fclose(tr_info[i].fptr);
        }
    }
}

int recv_file_name_length(struct transfer_info* tr_info, struct pollfd* pfd) {

    char filename_length_buf[5];
    long ret;

    ret = recv(pfd->fd, filename_length_buf, 4 * sizeof (char), 0);

    if (ret < 0) {
        if (errno != EWOULDBLOCK) {
            return -1;
        }
        return EWOULDBLOCK;
    }

    /*****************************************************/
    /* If the connection has been closed by the client   */
    /*****************************************************/
    if (ret == 0) {
        return CONN_CLOSED_BY_CLI;
    }

    filename_length_buf[5] = '\0';

    ret = atoi(filename_length_buf);

    /*****************************************************/
    /* Check for file_name_length validation             */
    /*****************************************************/
    if (ret <= 0 or ret > MAX_FULL_NAME_LENGTH) {
        return INVALID_NAME_LENGTH;
    }


    tr_info->file_name_length = ret;

    return SUCCESS;
}

int recv_file_name(struct transfer_info* tr_info, struct pollfd* pfd) {

    char filename_buf[tr_info->file_name_length + 1];
    long ret;

    /*****************************************************/
    /* Receiving file name from client                   */
    /*****************************************************/
    ret = recv(pfd->fd, filename_buf, tr_info->file_name_length, 0);

    if (ret < 0) {
        if (errno != EWOULDBLOCK) {
            return -1;
        }
        return EWOULDBLOCK;
    }

    /*****************************************************/
    /* If the connection has been closed by the client   */
    /*****************************************************/
    if (ret == 0) {
        return CONN_CLOSED_BY_CLI;
    }

    filename_buf[tr_info->file_name_length] = '\0';

    /*****************************************************/
    /* Try to allocate memory for file name              */
    /*****************************************************/
    tr_info->full_file_name = (char*) calloc(tr_info->file_name_length + 1, sizeof (char));

    if (tr_info->full_file_name == NULL) {
        return MEM_ALLOC_FAIL;
    }

    snprintf(tr_info->full_file_name, tr_info->file_name_length + 1, "%s", filename_buf);

    tr_info->full_file_name[tr_info->file_name_length] = '\0';

    tr_info->short_file_name = basename(tr_info->full_file_name);

    return SUCCESS;
}

int recv_file_cont(struct transfer_info* tr_info, struct pollfd* pfd) {

    int ret;
    char* filecontent_buf[BUFSIZE];

    /*****************************************************/
    /* Receiving file content from client                */
    /*****************************************************/
    ret = recv(pfd->fd, filecontent_buf, BUFSIZE, 0);

    printf("  recv_n = %ld\n", ret);

    if (ret < 0) {
        if (errno != EWOULDBLOCK) {
            return -1;
        }
        return EWOULDBLOCK;
    }

    /*****************************************************/
    /* If the connection has been closed by the client   */
    /*****************************************************/
    if (ret == 0) {
        return CONN_CLOSED_BY_CLI;
    }

    /*****************************************************/
    /* Write data to the file                            */
    /*****************************************************/
    ret = fwrite(filecontent_buf, 1, ret, tr_info->fptr);

    printf("  write_n = %ld\n", ret);

    if (ret <= 0) {
        return FILE_WR_ERR;
    }

    return SUCCESS;

}