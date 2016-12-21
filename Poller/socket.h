#ifndef _SOCKET__H
#define _SOCKET__H

#include <string>

#include "error.h"

/* TCP socket API wrapper. */

class Socket {
    int fd;
    std::string address;
    std::string port;

public:

    Socket(std::string const& inputAddress, int inputPort);
    Socket(std::string const& inputAddress, std::string const& inputPort);
    ~Socket();

    /* Reads at most size bytes from the socket and stores them to buffer. */
    size_t read(char* buffer, size_t size);

    /* Writes request to the socket */
    size_t write(std::string request);

    /* Read all available data from the socket */
    void readAll(std::string* response);

    /* reads line byte-by-byte from the socket until \\r\\n is detected or no more data are available. */
    size_t readLine(std::string* line);

    /* Exceptions */
    class ConnectionError;
    class IOError;

private:

    void open();
    void close();

    /* Read exactly one byte from the socket */
    bool readCharacter(char* buffer);

    bool isReadyToRead();
    bool isReadyToRead(struct timeval timeout);

};

/*
 *  This is thrown when the remote host wasn't found
 *  or when it doesn't accept the connection.
 */
class Socket::ConnectionError : public Error {
public:

    ConnectionError(std::string const& why) {
        problem = "Unable to connect";
        reason = why;
    }
};

/* Indicates problems with receiving/sending data. */
class Socket::IOError : public Error {
public:

    IOError(std::string const& what, std::string const& why) {
        problem = what;
        reason = why;
    }
};

#endif