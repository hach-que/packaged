/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_LOWLEVEL_FSRESULT
#define CLASS_LOWLEVEL_FSRESULT

#include <libpackaged-fs/config.h>

namespace AppLib
{
    namespace LowLevel
    {
        namespace FSResult
        {
            enum FSResult
            {
                E_SUCCESS,
                E_FAILURE_GENERAL,
                E_FAILURE_INVALID_FILENAME,
                E_FAILURE_INVALID_PATH,
                E_FAILURE_INVALID_POSITION,
                E_FAILURE_INODE_ALREADY_ASSIGNED,
                E_FAILURE_INODE_NOT_ASSIGNED,
                E_FAILURE_INODE_NOT_VALID,
                E_FAILURE_NOT_A_DIRECTORY,
                E_FAILURE_NOT_A_FILE,
                E_FAILURE_NOT_UNIQUE,
                E_FAILURE_NOT_IMPLEMENTED,
                E_FAILURE_MAXIMUM_CHILDREN_REACHED,
                E_FAILURE_PARTIAL_TRUNCATION,
                E_FAILURE_UNKNOWN
            };
        }
    }
}

#endif
