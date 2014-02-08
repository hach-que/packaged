/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_FREELIST
#define CLASS_FREELIST

#include <libpackaged-fs/config.h>

namespace AppLib
{
    namespace LowLevel
    {
        class FreeList;
    }
}

#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <libpackaged-fs/lowlevel/endian.h>
#include <libpackaged-fs/lowlevel/fs.h>

namespace AppLib
{
    namespace LowLevel
    {
        class FreeList
        {
        public:
            FreeList(FS * filesystem, BlockStream * fd);

            // Finds a free block, marks it as allocated in the free
            // space allocation table, and returns it's position for
            // writing.
            uint32_t allocateBlock();

            // Frees a specified block, marking it as unallocated in
            // the free space allocation table.
            void freeBlock(uint32_t pos);

            // Returns whether a specified position is free.
            bool isBlockFree(uint32_t pos);

            // Returns the specified type of an inode at the specified
            // position, returning INT_FREEBLOCK and INT_DATA in appropriate
            // circumstances.
            INodeType::INodeType getBlockType(uint32_t pos);

         private:
            FS * filesystem;
            BlockStream *fd;

            // Returns the position in the free space allocation table
            // where a 32-bit position integer can be written, that
            // currently matches pos.
            uint32_t getIndexInList(uint32_t pos);

            // Returns the position in the free space allocation table
            // where a 32-bit position integer can be written, that
            // currently matches pos.  Optionally providing an availPos
            // which is free space that has not yet been marked as free
            // (used as part of freeBlock to immediately repurpose free'd
            // blocks for use as part of the FreeList structure).  In the
            // event that this function reuses availPos for the freelist,
            // it returns 1.
            uint32_t getIndexInList(uint32_t pos, uint32_t availPos);

            // An in-memory cache of the free space allocation table, so
            // that fast lookups can be done to determine whether or not
            // a block is free or not (without having to cycle through the
            // entire free block allocation table).
            //
            // The first (key) value is the position that's free, the second
            // value is the position on disk of the free allocation index
            // (i.e. the result of getIndexInList for the specified position).
            std::map < uint32_t, uint32_t > position_cache;

            // Resyncronizes the cache based on what is on disk.
            void syncronizeCache();
        };
    }
}

#endif
