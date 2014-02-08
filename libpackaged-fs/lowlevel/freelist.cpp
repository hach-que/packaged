/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/lowlevel/freelist.h>
#include <libpackaged-fs/lowlevel/fs.h>
#include <math.h>

namespace AppLib
{
    namespace LowLevel
    {
        FreeList::FreeList(FS * filesystem, BlockStream * fd)
        {
            this->filesystem = filesystem;
            this->fd = fd;

            // Make a cache out of the on-disk data.
            this->syncronizeCache();
        }

        uint32_t FreeList::allocateBlock()
        {
            // Check to see if the size of the position cache is 0, in which case
            // we need to actually allocate a new block at the end of the file.
            if (this->position_cache.size() == 0)
            {
                // Get the filesize.
                std::streampos oldg = this->fd->tellg();
                this->fd->seekg(0, std::ios::end);
                uint32_t fsize = (uint32_t) this->fd->tellg();
                 this->fd->seekg(oldg);

                // Align the position on the upper 4096 boundary.
                double fblocks = fsize / 4096.0f;
                uint32_t alignedpos = ceil(fblocks) * 4096;

                // Force the block to be consumed so that the next time
                // we try to allocate a block, the seek-to-end-of-file
                // will work as expected.
                std::streampos oldp = this->fd->tellp();
                const char* zero = "\0";
                this->fd->seekp(alignedpos);
                for (int i = 0; i < 4096; i += 1)
                    this->fd->write(zero, 1);
                this->fd->seekp(oldp);

                Logging::showDebugW("FREELIST: Allocate (  new   ) block at %u.", alignedpos);

                 return alignedpos;
            }

            // Get the first unallocated block.
            std::map < uint32_t, uint32_t >::iterator i = this->position_cache.begin();

            // Update the position in the free block allocation table
            // to be equal to 0 to indicate that the free block is taken.
            std::streampos oldp = this->fd->tellp();
            this->fd->seekp(i->first);
            Endian::doW(this->fd, reinterpret_cast < char *>(&i->second), 4);
            this->fd->seekp(oldp);
            uint32_t res = i->second;

            Logging::showDebugW("FREELIST: Allocate (existing) block at %u.", res);

            // Remove the entry from the position cache.
            this->position_cache.erase(i);

            // Return the new writable position.
            return res;
        }

        void FreeList::freeBlock(uint32_t pos)
        {
            // Get a new, blank writable index on the disk (and if
            // we need to allocate a new block on disk for the freelist
            // tell it to use the one we are free'ing).
            uint32_t dpos = this->getIndexInList(0, pos);

            // Although hacky, getIndexInList returns 1 to indicate that
            // it used the provided availPos argument for it's new block.
            // In that case, since getIndexInList immediately reallocated
            // the new block, we're no longer actually going to free it
            // (since it is now used).
            if (dpos == 1)
            {
                Logging::showDebugW("FREELIST: Reallocated block at %u for list use.", pos);
                return;
            }

            // If we can actually store the free'd block on disk, do so.
            if (dpos != 0)
            {
                // Store the current position of the file descriptor.
                std::streampos oldg = this->fd->tellg();

                // Write to disk.
                this->fd->seekp(dpos);
                Endian::doW(this->fd, reinterpret_cast < char *>(&pos), 4);

                // Restore the position of the file descriptor.
                this->fd->seekg(oldg);

                Logging::showDebugW("FREELIST: Free block at %u.", pos);
            }
            else
                Logging::showDebugW("FREELIST: Unable to record free'd block %u on disk.", pos);

            // Add the new free position to the cache.
            this->position_cache.insert(std::map < uint32_t, uint32_t >::value_type(dpos, pos));
        }

        uint32_t FreeList::getIndexInList(uint32_t pos)
        {
            return this->getIndexInList(pos, 0);
        }

        uint32_t FreeList::getIndexInList(uint32_t pos, uint32_t availPos)
        {
            // Get the FSInfo inode by position.
            INode fsinfo = this->filesystem->getINodeByPosition(OFFSET_FSINFO);

            // Get the position of the first FreeList inode.
            uint32_t fpos = fsinfo.pos_freelist;
            uint32_t ipos = 0;
            uint32_t tpos = 0;

            // Store the current position of the file descriptor.
            std::streampos oldg = this->fd->tellg();

            // Loop through the FreeList inodes, searching for
            // correct index.
            while (fpos != 0)
            {
                for (int i = 8; i < 4096; i += 4)
                {
                    this->fd->seekg(fpos + i);
                    Endian::doR(this->fd, reinterpret_cast < char *>(&tpos), 4);
                    if (tpos == pos)
                    {
                        // Success, we've matched correctly.
                        // Seek back to the original position.
                        this->fd->seekg(oldg);

                        // Return the correct position.
                        return fpos + i;
                    }
                }

                // Once we have scanned all the entries in our current FreeList block, we need
                // to move onto the next one.
                this->fd->seekg(fpos + 4);
                Endian::doR(this->fd, reinterpret_cast < char *>(&fpos), 4);
            }

            // Special condition: If the pos is 0, and ipos is 0,
            // then we need to allocate a new FreeList block for
            // storing a new value.
            if (pos == 0 && ipos == 0)
            {
                // Create a new FreeList block.
                uint32_t fpos = availPos;
                if (fpos == 0)
                    fpos = this->allocateBlock();
                if (fpos == 0)
                {
                    this->fd->seekg(oldg);
                    return 0;
                }
                INode fnode(0, "", INodeType::INT_FREELIST);
                FSResult::FSResult res = this->filesystem->writeINode(fpos, fnode);
                if (res != FSResult::E_SUCCESS)
                {
                    this->fd->seekg(oldg);
                    return 0;
                }

                // Now assign the new FreeList block as the next one in
                // the list for the current last FreeList block.
                uint32_t lpos = fsinfo.pos_freelist;
                uint32_t llpos = lpos;
                if (lpos == 0)
                {
                    // Update FSInfo inode.
                    fsinfo.pos_freelist = fpos;

                    this->fd->seekp(OFFSET_FSINFO);
                    try
                    {
                        std::string data = fsinfo.getBinaryRepresentation();
                        this->fd->write(data.c_str(), data.size());
                    }
                    catch(std::ifstream::failure e)
                    {
                        // End-of-file.
                        this->fd->clear();
                        this->fd->seekg(oldg);
                        return 0;
                    }
                }
                else
                {
                    while (lpos != 0)
                    {
                        llpos = lpos;

                        // Get the next position.
                        INode node = this->filesystem->getINodeByPosition(lpos);
                        lpos = node.flst_next;
                    }

                    // Update FreeList inode.
                    INode onode = this->filesystem->getINodeByPosition(llpos);
                    onode.flst_next = fpos;
                    FSResult::FSResult res = this->filesystem->updateRawINode(onode, llpos);
                    if (res != FSResult::E_SUCCESS)
                    {
                        this->fd->seekg(oldg);
                        return 0;
                    }
                }

                // Seek back to the original position.
                this->fd->seekg(oldg);

                // If we used the availPos as our new freelist index,
                // return 1 instead of the normal value.
                if (fpos == availPos)
                    return 1;
                else
                    return fpos + HSIZE_FREELIST;
            }

            // Seek back to the original position.
            this->fd->seekg(oldg);

            return 0;
        }

        bool FreeList::isBlockFree(uint32_t pos)
        {
            for (std::map < uint32_t, uint32_t >::iterator i = this->position_cache.begin(); i != this->position_cache.end(); i++)
            {
                if (i->first == pos)
                    return true;
            }

            return false;
        }

        INodeType::INodeType FreeList::getBlockType(uint32_t pos)
        {
            return INodeType::INT_INVALID;
        }

        void FreeList::syncronizeCache()
        {
            // Clear the cache.
            this->position_cache.clear();

            // Get the FSInfo inode by position.
            INode fsinfo = this->filesystem->getINodeByPosition(OFFSET_FSINFO);

            // Get the position of the first FreeList inode.
            uint32_t fpos = fsinfo.pos_freelist;
            uint32_t ipos = 0;
            uint32_t tpos = 0;

            // Check to make sure there is at least one freelist block.
            if (fpos == 0)
                return;

            // Store the current position of the file descriptor.
            std::streampos oldg = this->fd->tellg();

            // Loop through the FreeList inodes, adding non-zero values
            // to the cache.
            while (fpos != 0)
            {
                for (int i = 8; i < 4096; i += 4)
                {
                    this->fd->seekg(fpos + i);
                    Endian::doR(this->fd, reinterpret_cast < char *>(&tpos), 4);
                    if (tpos != 0)
                    {
                        this->position_cache.insert(std::pair < uint32_t, uint32_t > (fpos + i, tpos));
                    }
                }

                // Get the next position.
                fpos = this->filesystem->getINodeByPosition(fpos).flst_next;
            }

            // Seek back to the original position.
            this->fd->seekg(oldg);

            // The cache has now been (re)built.
        }
    }
}
