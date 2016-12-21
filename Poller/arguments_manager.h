#ifndef _ARGUMENTS_MANAGER__H
#define _ARGUMENTS_MANAGER__H

#include <string>
#include <unistd.h>

#include "config.h"
#include "error.h"


class ArgumentsManager {
private:

    static const int MIN_PORT_RANGE = 1;
    static const int MAX_PORT_RANGE = 65535;
    static const int DEFAULT_PORT = DEF_PORT;

    int port;
    std::string username;
    std::string hostname;
    int messageId;

public:

    ArgumentsManager();

    /* Parses arguments from argv and stores them */
    void parse(int argc, char **argv);

    int getPort() const {
        return port;
    }

    std::string getUsername() const {
        return username;
    }

    std::string getHostname() const {
        return hostname;
    }

    int getMessageId() const {
        return messageId;
    }

    bool isMessageIdSet() const {
        return messageId != 0;
    }

    /* Exceptions */
    class GetoptError;
    class ArgumentDomainError;
    class MissingArgumentError;

private:
    static int str2int(std::string numberStoredInString);

    void setPort(char* optarg);
    void setHostname(char* optarg);
    void setUsername(char* optarg);
    void setMessageId(char* optarg);

    void checkMandatoryArguments() const;
};

class ArgumentsManager::GetoptError : public Error {
};

/* Argument of some option was out of it's domain. */
class ArgumentsManager::ArgumentDomainError : public Error {
public:

    ArgumentDomainError(std::string const& argument, std::string const& why) {
        problem = "Invalid argument -- " + argument;
        reason = why;
    }
};

/* Indicates missing of mandatory arguments. */
class ArgumentsManager::MissingArgumentError : public Error {
public:

    MissingArgumentError(std::string const& argument) {
        problem = "Missing argument";
        reason = argument + " is mandatory";
    }
};


#endif