/* vim: set ts=4 sw=4 tw=0 :*/

#ifndef CLASS_EXCEPTION_UTIL
#define CLASS_EXCEPTION_UTIL

#include <exception>

namespace AppLib
{
    namespace Exception
    {
        class InvalidOpenMode : public std::exception
        {
            virtual const char* what() const throw();
        };

        class NotSupported : public std::exception
        {
            virtual const char* what() const throw();
        };

        class NotImplemented : public std::exception
        {
            virtual const char* what() const throw();
        };
    }
}

#endif
