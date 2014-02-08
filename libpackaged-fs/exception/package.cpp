/* vim: set ts=4 sw=4 tw=0 :*/

#include <libpackaged-fs/exception/package.h>

namespace AppLib
{
    namespace Exception
    {
        const char* FileNotFound::what() const throw()
        {
            return "The specified file was not found.";
        }

        const char* PackageNotFound::what() const throw()
        {
            return "The specified package was not found.";
        }

        const char* PackageNotValid::what() const throw()
        {
            return "The specified package was not valid.";
        }

        const char* NoFreeSpace::what() const throw()
        {
            return "There was no free space on the containing medium with which to expand the package.";
        }

        const char* AccessDenied::what() const throw()
        {
            return "Access was denied to the specified file or directory.";
        }

        const char* PathNotValid::what() const throw()
        {
            return "The specified path was not in the correct format.";
        }

        const char* FileExists::what() const throw()
        {
            return "The specified file already exists.";
        }

        const char* NotADirectory::what() const throw()
        {
            return "The specified path is not a directory.";
        }

        const char* IsADirectory::what() const throw()
        {
            return "The specified path is a directory, but directories are not permitted "
                   "for this operation.";
        }

        const char* DirectoryNotEmpty::what() const throw()
        {
            return "The directory is not empty.";
        }

        const char* FileTooBig::what() const throw()
        {
            return "The specified file can not be increased to the required size.";
        }
    }
}

