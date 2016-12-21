#ifndef _ERROR__H
#define _ERROR__H

#include <stdexcept>
#include <string>

/* Base class for errors and exceptions. */

class Error : public std::exception {
protected:
    std::string problem;
    std::string reason;

public:
    Error(std::string what = "", std::string why = "");
    virtual ~Error() throw ();

    /* Return an error report: <problem>: <reason> */
    const char* what() const throw ();
};

#endif
