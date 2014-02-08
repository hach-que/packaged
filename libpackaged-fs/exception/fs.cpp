/* vim: set ts=4 sw=4 tw=0 :*/

#include <libpackaged-fs/exception/fs.h>

namespace AppLib
{
    namespace Exception
    {
        const char* INodeSaveInvalid::what() const throw()
        {
            return "Attempted to save UNSET or INVALID inode.  This usually "
                   "indicates the filesystem is corrupt.";
        }

        const char* INodeSaveFailed::what() const throw()
        {
            return "Unable to save inode data to disk.";
        }

        const char* INodeExhaustion::what() const throw()
        {
            return "Unable to save new data as there are no more available IDs "
                   "(inodes) for addressing content in the package.";
        }

        const char* InternalInconsistency::what() const throw()
        {
            return "An internal inconsistency occurred while reading or writing "
                   "to the package.  Close or unmount the package and check it "
                   "with the appropriate tool to ensure the package is not "
                   "corrupt.";
        }

        const char* DirectoryChildLimitReached::what() const throw()
        {
            return "No more files or subdirectories can be stored in the "
                   "specified directory due to filesystem limitations.";
        }

        const char* FilenameTooLong::what() const throw()
        {
            return "One or more of the path components are too long.";
        }
    }
}

