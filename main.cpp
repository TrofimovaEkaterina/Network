#include <stdio.h>
#include <iostream>
#include <sys/types.h>


#include <map>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>

#include "opt.h"

using namespace std;

int main(int argc, char*argv[]) {

    printf("Application started\n");

    int send_bc_sock, list_bc_sock, status, sinlen;
    struct sockaddr_in sock_in;
    int enable = 1;

    sinlen = sizeof (struct sockaddr_in);
    memset(&sock_in, 0, sinlen);

    map<in_addr_t, long> Map; //map for IP;time preserving
    map<in_addr_t, long>::iterator it;

    /*TCP almost always uses SOCK_STREAM and UDP uses SOCK_DGRAM.

    TCP/SOCK_STREAM is a connection-based protocol. The connection is established 
    and the two parties have a conversation until the connection is terminated 
    by one of the parties or by a network error.

    UDP/SOCK_DGRAM is a datagram-based protocol. You send one datagram and get 
    one reply and then the connection terminates.

        If you send multiple packets, TCP promises to deliver them in order. 
        UDP does not, so the receiver needs to check them, if the order matters.

        If a TCP packet is lost, the sender can tell. Not so for UDP.

        UDP datagrams are limited in size, from memory I think it is 512 bytes. 
        TCP can send much bigger lumps than that.

        TCP is a bit more robust and makes more checks. UDP is a shade lighter 
        weight (less computer and network stress).
     */

    /*to send broadcast datagrams*/
    send_bc_sock = socket(PF_INET, SOCK_DGRAM, 0); //PF_INET = AF_INET

    sock_in.sin_addr.s_addr = htonl(INADDR_ANY); ///////////////
    sock_in.sin_port = htons(DEF_SEND_BC_PORT);
    sock_in.sin_family = PF_INET;

    status = bind(send_bc_sock, (struct sockaddr *) &sock_in, sinlen);
    if (status != 0) {
        close(send_bc_sock);
        error("Bind(): ", errno);
    }


    status = setsockopt(send_bc_sock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof (enable)); /*on the broadcast function on send_bc_sock*/
    if (status != 0) {
        close(send_bc_sock);
        error("Setsockopt(): ", errno);
    }

    /*to listen incoming broadcast datagrams*/
    list_bc_sock = socket(PF_INET, SOCK_DGRAM, 0);

    sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_in.sin_port = htons(DEF_LIST_BC_PORT);
    sock_in.sin_family = PF_INET;

    status = bind(list_bc_sock, (struct sockaddr *) &sock_in, sinlen);
    if (status != 0) {
        close(list_bc_sock);
        close(send_bc_sock);
        error("Bind(): ", errno);
    }

    status = setsockopt(list_bc_sock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof (enable)); /*on the broadcast function on list_bc_sock*/
    if (status != 0) {
        close(list_bc_sock);
        close(send_bc_sock);
        error("Setsockopt(): ", errno);
    }

    /* -1 = 255.255.255.255 this is a BROADCAST address,
       a local broadcast address could also be used.
       you can comput the local broadcat using NIC address and its NETMASK 
     */

    sock_in.sin_addr.s_addr = htonl(-1); /* send message to IP = 255.255.255.255 */
    sock_in.sin_port = htons(DEF_LIST_BC_PORT); /* and corresponding port number */
    sock_in.sin_family = PF_INET;

    sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof (client_addr);

    struct timespec recv_time, start_time, end_time;
    struct timeval tv;

    char incom_msg[MSG_SIZE];
    char outcom_msg[MSG_SIZE] = "Hello";

    fd_set readfds;

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (true) {

        int nbytes = sendto(send_bc_sock, outcom_msg, sizeof (outcom_msg), 0, (struct sockaddr *) &sock_in, sinlen);

        if (nbytes < 0) {
            perror("Recvfrom(): ");
            fprintf(stderr, "Terminating\n");
            break;
        }

        FD_ZERO(&readfds);
        FD_SET(list_bc_sock, &readfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int rv = select(list_bc_sock + 1, &readfds, NULL, NULL, &tv);

        if (rv < 0) {
            perror("Select(): ");
            fprintf(stderr, "Terminating\n");
            break;
        }


        if (rv == 1) {
            int nbytes = recvfrom(list_bc_sock, &incom_msg, sizeof (incom_msg) - 1, 0, (struct sockaddr *) & client_addr, &client_addr_size);
            clock_gettime(CLOCK_MONOTONIC, &recv_time);

            if (nbytes < 0) {
                perror("Recvfrom(): ");
                fprintf(stderr, "Terminating\n");
                break;
            }


            if (nbytes > 0) {
                /*Insert new element (new client IP) into Map or change time, when the client made the last broadcast*/
                it = Map.find(client_addr.sin_addr.s_addr);
                if (it != Map.end()) {
                    /*el was found -> change time*/
                    Map.at(client_addr.sin_addr.s_addr) = recv_time.tv_sec;
                } else {
                    Map.insert(pair<in_addr_t, long>(client_addr.sin_addr.s_addr, recv_time.tv_sec));
                    printf("New instance available. Total: %u\n", Map.size());
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &end_time);


        /*every 5 sec sends broadcast msg and cheacks if some client has become diseabled*/
        if (end_time.tv_sec - start_time.tv_sec > 5) {
            int nbytes = sendto(send_bc_sock, outcom_msg, sizeof (outcom_msg), 0, (struct sockaddr *) &sock_in, sinlen);

            if (nbytes < 0) {
                perror("Recvfrom(): ");
                fprintf(stderr, "Terminating\n");
                break;
            }

            for (it = Map.begin(); it != Map.end(); it++) {
                if (end_time.tv_sec - it->second > 10) {
                    Map.erase(it);
                    printf("One instance went down. Total: %u\n", Map.size());
                }
            }
            printf("Total instances: %u\n", Map.size());
            clock_gettime(CLOCK_MONOTONIC, &start_time);
        }

    }

    shutdown(send_bc_sock, 2);
    shutdown(list_bc_sock, 2);
    close(send_bc_sock);
    close(list_bc_sock);
}