/* vim: set ts=4 sw=4 tw=0 :*/

#ifndef CLASS_EXCEPTION_PACKAGE
#define CLASS_EXCEPTION_PACKAGE

#include <exception>

namespace AppLib
{
    namespace Exception
    {
        class FileNotFound : public std::exception
        {
            virtual const char* what() const throw();
        };

        class PackageNotFound : public std::exception
        {
            virtual const char* what() const throw();
        };

        class PackageNotValid : public std::exception
        {
            virtual const char* what() const throw();
        };

        class NoFreeSpace : public std::exception
        {
            virtual const char* what() const throw();
        };

        class AccessDenied : public std::exception
        {
            virtual const char* what() const throw();
        };

        class PathNotValid : public std::exception
        {
            virtual const char* what() const throw();
        };

        class FileExists : public std::exception
        {
            virtual const char* what() const throw();
        };

        class NotADirectory : public std::exception
        {
            virtual const char* what() const throw();
        };

        class IsADirectory : public std::exception
        {
            virtual const char* what() const throw();
        };

        class DirectoryNotEmpty : public std::exception
        {
            virtual const char* what() const throw();
        };

        class FileTooBig : public std::exception
        {
            virtual const char* what() const throw();
        };
    }
}

#endif
