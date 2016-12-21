#include "imap.h"

#include <iostream>
#include <sstream>
#include <string>
#include <string.h>
#include <stdlib.h>

#include "socket.h"

ImapSession::ImapSession() : socket(NULL) {
    curCommandId = "0000";
}

ImapSession::ImapSession(std::string const& server, int port) {
    curCommandId = "0000";
    open(server, port);
}

ImapSession::~ImapSession() {
    close();
}

void ImapSession::sendCommand(std::string const& command) {
    long int cCI = strtol(curCommandId.c_str(), NULL, 10);
    cCI++;

    std::stringstream ss;

    if (cCI < 10) {

        ss << "000";
        ss << cCI;
        curCommandId = ss.str();

    }

    if (cCI >= 10 && cCI < 100) {

        ss << "00";
        ss << cCI;
        curCommandId = ss.str();

    }

    if (cCI >= 100 && cCI < 1000) {

        ss << "0";
        ss << cCI;
        curCommandId = ss.str();

    }

    if (cCI >= 1000 && cCI < 10000) {

        ss << cCI;
        curCommandId = ss.str();

    }

    if (cCI >= 10000) {
    }//////////////////////////////////////////////////////////////////////////////////

    socket->write(curCommandId + " " + command + "\r\n");
}

void ImapSession::getConnResponse(ServerResponse* response) {

    std::string buffer;
    socket->readLine(&buffer);

    response->status = "UNDEF";

    //Если мы принимаем ответ на коннект ("привет" от сервера)
    if (buffer.compare(2, 2, "OK") == 0) response->status = "OK";
    if (buffer.compare(2, 2, "NO") == 0) response->status = "NO";
    if (response->status == "UNDEF") response->status = "BAD";

    response->statusMessage = buffer;
    response->data.clear();
}

void ImapSession::getCommResponse(ServerResponse* response) {
    std::string buffer;
    int bytesRead;

    response->status = "UNDEF";

    while (true) {
        buffer.clear();

        bytesRead = socket->readLine(&buffer);

        if (bytesRead >= 7) {
            if (buffer.compare(0, 4, curCommandId) == 0) { //"0001 OK ..."
                buffer.erase(0, 5);

                if (buffer.compare(0, 2, "OK") == 0) response->status = "OK";
                if (buffer.compare(0, 2, "NO") == 0) response->status = "NO";
                if (response->status == "UNDEF") response->status = "BAD";

                response->data.push_back(buffer);
                response->statusMessage = buffer;

                break;
            }
        }

        response->data.push_back(buffer);
    }

}

void ImapSession::open(std::string const& server, int port) {

    socket = new Socket(server, port);

    ServerResponse welcomeMessage;

    getConnResponse(&welcomeMessage);

    if (welcomeMessage.status != "OK") {
        throw ServerError("Conection refused", welcomeMessage.statusMessage);
    }
}

void ImapSession::close() {
    if (socket != NULL) {
        sendCommand("logout");

        ServerResponse quitACK;
        getCommResponse(&quitACK);

        delete socket;
    }
}

void ImapSession::authenticate(std::string const& username, std::string const& password) {
    ServerResponse response;

    sendCommand("login " + username + " " + password);
    getCommResponse(&response);

    if (response.status != "OK") {
        throw ServerError("Authentication failed", response.statusMessage);
    }
}

void ImapSession::printMessageList() {
    ServerResponse response;

    //sendCommand("LIST ""/"" *");

    sendCommand("SELECT INBOX");

    getCommResponse(&response);

    if (response.status != "OK") {
        throw ServerError("Unable to retrieve message list", response.statusMessage);
    }

    for (std::list<std::string>::iterator line = response.data.begin(); line != response.data.end(); line++) {
        std::cout << *line << std::endl;
    }
}

void ImapSession::printMessage(int messageId) {
    ServerResponse response;

    std::stringstream ss;
    ss << messageId;
    
    sendCommand("FETCH " + ss.str() + " BODY[HEADER]");

    getCommResponse(&response);
    if (response.status != "OK") {
        throw ServerError("Unable to retrieve requested message", response.statusMessage);
    }

    for (std::list<std::string>::iterator line = response.data.begin(); line != response.data.end(); line++) {
        std::cout << *line << std::endl;
    }
}