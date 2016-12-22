/*
 * @brief BSD sockets API adapter.
 */

#include "config.h"
#include "socket.h"
#include "error.h"

#include <string>
#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <string.h>

Socket::Socket(std::string const& inAddress, std::string const& inPort) {
    fd = -1;

    address = inAddress;
    port = inPort;

    open();
}

Socket::Socket(std::string const& inAddress, int inPort) {
    fd = -1;

    std::stringstream portInString;
    portInString << inPort;

    address = inAddress;
    port = portInString.str();

    open();
}

void Socket::open() {

    struct addrinfo hints;
    struct addrinfo *res, *resPtr;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    std::cout << "Get addr info" << std::endl;

    int ret = getaddrinfo(address.c_str(), port.c_str(), &hints, &res);

    std::cout << "Get addr info WORK" << std::endl;

    if (ret != 0) {
        throw ConnectionError(gai_strerror(ret));
    }

    /*check all result addresses*/
    for (resPtr = res; resPtr != NULL; resPtr = resPtr->ai_next) {

        std::cout << "SOCKET AND CONNECT" << std::endl;

        fd = ::socket(resPtr->ai_family, resPtr->ai_socktype, resPtr->ai_protocol);
        if (fd == -1) {
            continue;
            std::cout << "SOCKET FAILED" << std::endl;

        }

        if (connect(fd, resPtr->ai_addr, resPtr->ai_addrlen) != -1) {
            std::cout << "SOCKET AND CONNECT!!!!!!!!!!!!!!!!!!" << std::endl;
            break;
        }
        /*
                int ret;
                ret = select(sockfd + 1, NULL, &wfds, NULL, NULL); //should use timeout
                if (ret == 1 && getSocketOpt(sockfd, SO_ERROR) == 0) {
                    return 0; //successfully connected
                }
         */

        std::cout << "CONNECT FAILED" << std::endl;

        ::close(fd);
    }

    std::cout << "NEAR EXIT" << std::endl;
            
    if (resPtr == NULL) {
        throw ConnectionError("Cannot establish connection to the server");
    }

    ::freeaddrinfo(res);
}

Socket::~Socket() {
    if (fd > 0) {
        close();
    }
}

void Socket::close() {
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
}

size_t Socket::read(char* buffer, size_t size) {

    if (!isReadyToRead()) {
        throw IOError("Recieving error", "Server not responding (connection timed out).");
    }

    ssize_t bytesRead = ::read(fd, buffer, size);
    if (bytesRead < 0) {
        throw IOError("Recieving error", "Unable to resolve data from remote host");
    }

    return bytesRead;
}

size_t Socket::write(std::string request) {

    ssize_t bytesRead = ::write(fd, request.c_str(), request.length());
    if (bytesRead < 0) {
        throw IOError("Sending error", "Unable to send data to remote host");
    }

    return bytesRead;
}

void Socket::readAll(std::string *response) {
    char buffer[BLOCK_SIZE];
    struct timeval timeout;

    if (!isReadyToRead()) {
        throw IOError("Recieving error", "Server not responding (connection timed out).");
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    while (isReadyToRead(timeout)) {
        ssize_t bytesRead = ::read(fd, buffer, BLOCK_SIZE);
        *response += buffer;

        if (bytesRead < BLOCK_SIZE) break;
    }
}

bool Socket::readCharacter(char* buffer) {
    ssize_t bytesRead = read(buffer, 1);

    if (bytesRead > 0) {
        return true;
    }
    return false;
}

size_t Socket::readLine(std::string* line) {
    char buffer[2] = "";
    line->clear();
    size_t bytesRead = 0;

    if (readCharacter(&(buffer[1]))) {

        do {
            *line += buffer[0]; // add char to string
            buffer[0] = buffer[1];
            bytesRead++;
        } while (readCharacter(&(buffer[1])) && !(buffer[0] == '\r' && buffer[1] == '\n'));

        line->erase(0, 1);
        bytesRead--;
    }

    return bytesRead;
}

bool Socket::isReadyToRead() {
    fd_set readSet;
    struct timeval timeout;
    int ret;

    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);

    /* 30 seconds timeout */
    timeout.tv_sec = SOCKET_READ_TIMEOUT;
    timeout.tv_usec = 0;

    ret = select(fd + 1, &readSet, NULL, NULL, &timeout);

    if (ret > 0) {
        return true;
    }

    return false;
}

bool Socket::isReadyToRead(struct timeval timeout) {
    fd_set readSet;
    int ret;

    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);

    ret = select(fd + 1, &readSet, NULL, NULL, &timeout);

    if (ret > 0) {
        return true;
    }

    return false;
}