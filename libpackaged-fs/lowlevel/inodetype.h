/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_LOWLEVEL_INODETYPE
#define CLASS_LOWLEVEL_INODETYPE

#include <libpackaged-fs/config.h>

namespace AppLib
{
    namespace LowLevel
    {
        // WARN: The values here are also used to store the types
        //       in the actual AppFS packages.  Therefore you should
        //       not change any values since it will break the
        //       ability to read existing packages.
        namespace INodeType
        {
            enum INodeType
            {
                // Free Block (unused in disk images)
                INT_FREEBLOCK = 0,

                // File Information Block
                INT_FILEINFO = 1,
                // Segment Information Block
                INT_SEGINFO = 2,
                // Data Block (unused in disk images)
                INT_DATA = 254,

                // Directory Block
                INT_DIRECTORY = 3,

                // Symbolic Link
                INT_SYMLINK = 4,
                // Hard Link
                INT_HARDLINK = 5,
                // Special File (FIFO / Device)
                INT_DEVICE = 10,

                // Temporary Block
                INT_TEMPORARY = 6,
                // Free Space Allocation Block
                INT_FREELIST = 7,
                // Filesystem Information Block
                INT_FSINFO = 8,

                // Invalid and Unset Blocks (unused in disk images)
                INT_INVALID = 9,
                INT_UNSET = 255
            };
        }
    }
}

#endif
