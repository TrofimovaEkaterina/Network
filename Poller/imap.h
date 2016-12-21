#ifndef IMAP_H
#define IMAP_H

#include <list>
#include <string>

#include "error.h"

class Socket; /* Forward-declaration. */

/*
 *  This class is responsible for communication with the server using IMAP
 *  over TCP. It sends IMAP commands to the server and process its responses.
 */
class ImapSession {
    Socket* socket;
    std::string curCommandId;

public:
    ImapSession();
    ImapSession(std::string const& server, int port);
    ~ImapSession();

    void authenticate(std::string const& username, std::string const& password);

    void printMessageList();
    void printMessage(int messageId);

    /* Exceptions */
    class ServerError;

private:
    struct ServerResponse;

    void sendCommand(std::string const& command);

    void getConnResponse(ServerResponse* response);
    
    void getCommResponse(ServerResponse* response);

    void open(std::string const& server, int port);
    void close();
};

/* This is used internaly by ImapSession to store server's responses. */
struct ImapSession::ServerResponse {
    std::string status;             /*< OK, NO, BAD */
    std::string statusMessage;
    std::list<std::string> data;    /*< Multi-line data in case, they were present. */
};

class ImapSession::ServerError : public Error {
public:

    ServerError(std::string const& what, std::string const& serverStatus) {
        problem = what;
        reason = serverStatus;
    }
};


#endif /* IMAP_H */

