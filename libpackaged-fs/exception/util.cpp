/* vim: set ts=4 sw=4 tw=0 :*/

#include <libpackaged-fs/exception/util.h>

namespace AppLib
{
    namespace Exception
    {
        const char* InvalidOpenMode::what() const throw()
        {
            return "The open mode must be either \"r\", \"w\" or \"rw\".";
        }

        const char* NotSupported::what() const throw()
        {
            return "The specified operation is not supported.";
        }

        const char* NotImplemented::what() const throw()
        {
            return "The specified functionality is not implemented.";
        }
    }
}

