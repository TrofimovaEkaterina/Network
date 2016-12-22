#include "arguments_manager.h"

#include <string>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

ArgumentsManager::ArgumentsManager() {
}

void ArgumentsManager::parse(int argc, char **argv) {

    int option = 0;

    /* Defaults */
    protocol = UNDECLARED;
    port = DEFAULT_PORT;
    hostname = "";
    username = "";
    messageId = 0;

    while ((option = getopt(argc, argv, "P:h:p:u:")) != -1) {
        switch (option) {
            case 'P': /* Protocol */
                setProtocol(optarg);
                break;
            case 'h': /* Hostname */
                setHostname(optarg);
                break;
            case 'p': /* Port number */
                setPort(optarg);
                break;
            case 'u': /* Username */
                setUsername(optarg);
                break;
            default:
                throw GetoptError();
                break;
        }
    }

    /* id argument */
    if (optind < argc) {
        setMessageId(argv[optind]);
    }

    checkMandatoryArguments();
}

void ArgumentsManager::checkMandatoryArguments() const {

    if (protocol == UNDECLARED) {
        throw MissingArgumentError("-P");
    }

    if (hostname.length() <= 0) {
        throw MissingArgumentError("-h");
    }

    if (username.length() <= 0) {
        throw MissingArgumentError("-u");
    }
}

int ArgumentsManager::str2int(std::string str) {
    return static_cast<int> (atoi(str.c_str()));
}

void ArgumentsManager::setProtocol(char* optarg) {
    if (strcmp(optarg, "pop3") == 0) {
        protocol = POP3;
        return;
    }

    if (strcmp(optarg, "imap") == 0) {
        protocol = IMAP;
        return;
    }

    throw ArgumentDomainError("-P", "Unknown protocol. Protocols in use are: pop3, imap");

};

void ArgumentsManager::setPort(char* optarg) {
    port = str2int(std::string(optarg));

    if (port < MIN_PORT_RANGE || port > MAX_PORT_RANGE) {
        throw ArgumentDomainError("-p", "Value out of range (1 ~ 65535)");
    }
}

void ArgumentsManager::setHostname(char* optarg) {
    hostname = std::string(optarg);
}

void ArgumentsManager::setUsername(char* optarg) {
    username = std::string(optarg);
}

void ArgumentsManager::setMessageId(char* optarg) {
    messageId = str2int(optarg);

    if (messageId <= 0) {
        throw ArgumentDomainError("id", "Message id must be number greater than 0");
    }
}