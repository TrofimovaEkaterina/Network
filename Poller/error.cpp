#include <stdexcept>
#include <string>
#include <error.h>
#include <errno.h>

#include "config.h"
#include "error.h"

Error::Error(std::string what, std::string why) {
    problem = what;
    reason = why;
}

Error::~Error() throw () {
}

const char* Error::what() const throw () {
    std::string message;

    if (problem.length() > 0) {
        message += problem;
    }

    if (reason.length() > 0) {
        message += ": " + reason;
    }

    return message.c_str();
}