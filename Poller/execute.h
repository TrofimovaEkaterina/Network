#ifndef EXECUTE_H
#define EXECUTE_H

#include <iostream>
#include <string>

#include <cstdlib>
#include <cstdio>

#include <termios.h>

#include "arguments_manager.h"
#include "pop3.h"
#include "imap.h"

std::string getPassword();
void usage(int status, char* programName);
int pop3_execute(std::string password, ArgumentsManager* arguments);
int imap_execute(std::string password, ArgumentsManager* arguments);

#endif /* EXECUTE_H */

