#include "execute.h"
#include "config.h"

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
        std::cout << "Password: ";
        password = getPassword();
        std::cout << std::endl;
    } catch (std::exception& error) {
        std::cerr << error.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Password got" << std::endl;

    switch (arguments.getProtocol()) {
        case POP3:
            pop3_execute(password, &arguments);
            break;
        case IMAP:
            imap_execute(password, &arguments);
            break;
        default:
            break;
    }


    return EXIT_SUCCESS;
}