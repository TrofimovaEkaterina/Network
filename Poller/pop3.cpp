#include <iostream>
#include <sstream>

#include "pop3.h"
#include "socket.h"

Pop3Session::Pop3Session() : socket(NULL) {
}

Pop3Session::Pop3Session(std::string const& server, int port) {
    open(server, port);
}

Pop3Session::~Pop3Session() {
    close();
}

void Pop3Session::sendCommand(std::string const& command) {
    socket->write(command + "\r\n");
}

void Pop3Session::getResponseStatusPart(ServerResponse* response) {

    std::string line;
    socket->readLine(&line);

    (line[0] == '+') ? (response->status = true) : (response->status = false);

    response->statusMessage = line;
    response->data.clear();
}

void Pop3Session::getResponseDataPart(ServerResponse* response) {
    std::string line;
    int numRead;

    while (true) {
        line.clear();

        numRead = socket->readLine(&line);

        if (line == "." || numRead == 0) {
            break;
        } else {
            response->data.push_back(line);
        }
    }
}

void Pop3Session::open(std::string const& server, int port) {

    socket = new Socket(server, port);

    ServerResponse welcomeMessage;
    getResponseStatusPart(&welcomeMessage);

    if (!welcomeMessage.status) {
        throw ServerError("Conection refused", welcomeMessage.statusMessage);
    }
}

void Pop3Session::close() {
    if (socket != NULL) {
        
        sendCommand("QUIT");

        ServerResponse quitACK;
        getResponseStatusPart(&quitACK);

        /* Maybe warning when QUIT fails? */

        delete socket;
    }
}

void Pop3Session::authenticate(std::string const& username, std::string const& password) {
    ServerResponse response;

    sendCommand("USER " + username);
    getResponseStatusPart(&response);

    if (!response.status) {
        throw ServerError("Authentication failed", response.statusMessage);
    }

    sendCommand("PASS " + password);
    getResponseStatusPart(&response);

    if (!response.status) {
        throw ServerError("Authentication failed", response.statusMessage);
    }
}

void Pop3Session::printMessageList() {
    ServerResponse response;

    sendCommand("LIST");
    getResponseStatusPart(&response);

    if (!response.status) {
        throw ServerError("Unable to retrieve message list", response.statusMessage);
    }

    getResponseDataPart(&response);

    if (response.data.size() == 0) {
        std::cout << "No messages available on the server." << std::endl;
    }

    for (std::list<std::string>::iterator line = response.data.begin(); line != response.data.end(); line++) {
        std::cout << *line << std::endl;
    }
}

void Pop3Session::printMessage(int messageId) {
    ServerResponse response;

    sendCommand("RETR " + messageId);
    getResponseStatusPart(&response);
    
    if (!response.status) {
        throw ServerError("Unable to retrieve requested message", response.statusMessage);
    }

    getResponseDataPart(&response);

    for (std::list<std::string>::iterator line = response.data.begin(); line != response.data.end(); line++) {
        std::cout << *line << std::endl;
    }
}