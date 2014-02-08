/* vim: set ts=4 sw=4 tw=0 :*/

#ifndef CLASS_EXCEPTION_FS
#define CLASS_EXCEPTION_FS

#include <exception>

namespace AppLib
{
    namespace Exception
    {
        class INodeSaveInvalid : public std::exception
        {
            virtual const char* what() const throw();
        };

        class INodeSaveFailed : public std::exception
        {
            virtual const char* what() const throw();
        };

        class INodeExhaustion : public std::exception
        {
            virtual const char* what() const throw();
        };

        class InternalInconsistency : public std::exception
        {
            virtual const char* what() const throw();
        };

        class DirectoryChildLimitReached : public std::exception
        {
            virtual const char* what() const throw();
        };

        class FilenameTooLong : public std::exception
        {
            virtual const char* what() const throw();
        };
    }
}

#endif
