#ifndef _POP3SESSION__H
#define _POP3SESSION__H

#include <list>
#include <string>

#include "error.h"

class Socket; /* Forward-declaration. */

/*
 *  This class is responsible for communication with the server using POP3
 *  over TCP. It sends POP3 commands to the server and process its responses.
 */
class Pop3Session {
    Socket* socket;

public:
    Pop3Session();
    Pop3Session(std::string const& server, int port);
    ~Pop3Session();

    void authenticate(std::string const& username, std::string const& password);

    void printMessageList();
    void printMessage(int messageId);

    /* Exceptions */
    class ServerError;

private:
    struct ServerResponse;

    void sendCommand(std::string const& command);

    void getResponseStatusPart(ServerResponse* response);
    void getResponseDataPart(ServerResponse* response);

    void open(std::string const& server, int port);
    void close();
};

/* To store server's responses. */
struct Pop3Session::ServerResponse {
    bool status;                    /* It's true on +OK, false on -ERR */
    std::string statusMessage;
    std::list<std::string> data;    /* Multi-line data in case, they were present. */
};

class Pop3Session::ServerError : public Error {
public:

    ServerError(std::string const& what, std::string const& serverStatus) {
        problem = what;
        reason = serverStatus;
    }
};

#endif