#include <iostream>
#include <string>

#include <cstdlib>
#include <cstdio>
#include <termios.h>

#include "config.h"
#include "error.h"
#include "arguments_manager.h"
#include "pop3.h"

std::string getPassword() {
    struct termios oldTerminalFlags, newTerminalFlags;

    int stdin_fd = fileno(stdin);

    if (tcgetattr(stdin_fd, &oldTerminalFlags) != 0) {
        throw std::runtime_error("Unable to set terminal flags");
    }

    newTerminalFlags = oldTerminalFlags;

    /*Terminal ECHO is turned off while reading the password.*/
    newTerminalFlags.c_lflag &= ~(ECHO);

    /* 
     * The change occurs after all output written to the file descriptor has been transmitted, 
     * and all input so far received but not read is discarded before the change is made.
     */
    if (tcsetattr(stdin_fd, TCSAFLUSH, &newTerminalFlags) != 0) {
        throw std::runtime_error("Unable to set terminal flags");
    }

    /* Read the password. */
    std::string password;
    std::getline(std::cin, password);

    /* Restore terminal. */
    tcsetattr(stdin_fd, TCSAFLUSH, &oldTerminalFlags);

    return password;
}

void usage(int status, char* programName) {

    std::cerr << "Usage: " << programName << " -h hostname [-p port] -u username  [id]" << std::endl;
    std::cerr << "       -h hostname     remote IP address or hostname" << std::endl;
    std::cerr << "       -p port         remote TCP port" << std::endl;
    std::cerr << "       -u username     username" << std::endl;
    std::cerr << "       id              id of the message to download" << std::endl;

    exit(status);
}

int main(int argc, char **argv) {

    ArgumentsManager arguments;

    /* Try to process cli arguments */
    try {
        arguments.parse(argc, argv);
    } catch (ArgumentsManager::GetoptError& error) {
        usage(EXIT_FAILURE, argv[0]);
    } catch (Error& error) {
        std::cerr << error.what() << std::endl;
        usage(EXIT_FAILURE, argv[0]);
    }

    /* Get password. */
    std::string password;
    try {
        std::cout << "Password : ";
        password = getPassword();
        std::cout << std::endl;
    } catch (std::exception& error) {
        std::cerr << error.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Password got" << std::endl;

    /* Process user's request. */
    try {
        std::cout << "Create session" << std::endl;
        Pop3Session pop3(arguments.getHostname(), arguments.getPort());
        
        std::cout << "Authentification..." << std::endl;
        pop3.authenticate(arguments.getUsername(), password);

        std::cout << "Yey!" << std::endl;

        password.clear(); // Remove password from memory

        /* print the list of available messages or print some specific message. */
        if (arguments.isMessageIdSet()) {
            pop3.printMessage(arguments.getMessageId());
        } else {
            pop3.printMessageList();
        }
    } catch (Error& error) {
        std::cerr << error.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}