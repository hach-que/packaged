/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_LOWLEVEL_FS
#define CLASS_LOWLEVEL_FS

#include <libpackaged-fs/config.h>

namespace AppLib
{
    namespace LowLevel
    {
        class FS;
    }
}

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <libpackaged-fs/lowlevel/endian.h>
#include <libpackaged-fs/fsfile.h>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <libpackaged-fs/lowlevel/inode.h>
#include <libpackaged-fs/lowlevel/freelist.h>
#include <libpackaged-fs/lowlevel/fsresult.h>

namespace AppLib
{
    namespace LowLevel
    {
        class FS
        {
        public:
            FS(LowLevel::BlockStream * fd);

            //! Returns whether the file descriptor is valid.  If this
            //! is false, and you call one of the functions in the class
            //! (other than getTemporaryNode or isValid), the program will
            //! use assert() to ensure the safety of the contents of the
            //! package.
            bool isValid();

            //! Writes an INode to the specified position and then
            //! updates the inode lookup table.
            FSResult::FSResult writeINode(uint32_t pos, INode node);

            //! Updates an INode.  The node must exist in the inode
            //! position lookup table.
            FSResult::FSResult updateINode(INode node);

            //! Updates a raw INode (such as a freelist block).
            FSResult::FSResult updateRawINode(INode node, uint32_t pos);

            //! Retrieves an INode by an ID.
            INode getINodeByID(uint16_t id);

            //! Retrieves an INode by an ID, without performing hardlink
            //! resolution.
            INode getRealINodeByID(uint16_t id);

            //! Retrieves an INode by position.
            INode getINodeByPosition(uint32_t pos);

            //! Retrieves the real INode by position (in the case of hardlinks).
            INode getINodeByRealPosition(uint32_t rpos);

            //! A return value of 0 indicates that the specified INode
            //! does not exist.  Underlyingly calls getINodePositionByID(id, true);
            uint32_t getINodePositionByID(uint16_t id);

            //! A return value of 0 indicates that the specified INode
            //! does not exist.
            uint32_t getINodePositionByID(uint16_t id, bool resolveHardlinks);

            //! Sets the position of an inode in the inode lookup table.
            FSResult::FSResult setINodePositionByID(uint16_t id, uint32_t pos);

            //! Find first free block and return that position.  The user specifies
            //! INT_FILE or INT_DIRECTORY to indicate the number of sequential free
            //! blocks to find.  A return value of 0 indicates that the file was either
            //! at the maximum filesize, or the function could not otherwise find
            //! a free block.
            uint32_t getFirstFreeBlock(INodeType::INodeType type);

            //! Find the first free inode number and return it.  A return value of 0
            //! indicates that there are no free inode numbers available (we can use
            //! a value of 0 since the root inode will always exist and will always
            //! have an ID of 0).
            uint16_t getFirstFreeINodeNumber();

            //! Returns whether the specified block is free according to the freelist.
            bool isBlockFree(uint32_t pos);

            //! Adds a child inode to a parent (directory) inode.  Please note that it doesn't
            //! check to see whether or not the child is already attached to the parent, but
            //! it will add the child reference in the lowest available slot.
            FSResult::FSResult addChildToDirectoryINode(uint16_t parentid, uint16_t childid);

            //! Removes a child inode from a parent (directory) inode.
            FSResult::FSResult removeChildFromDirectoryINode(uint16_t parentid, uint16_t childid);

            //! Returns whether or not a specified filename is unique
            //! inside a directory.  E_SUCCESS indicates unique, E_FAILURE_NOT_UNIQUE
            //! indicates not unique.
            FSResult::FSResult filenameIsUnique(uint16_t parentid, std::string filename);

            //! Returns an std::vector<INode> list of children within
            //! the specified directory.
            std::vector < INode > getChildrenOfDirectory(uint16_t parentid);

            /*! Returns an INode for the child with the specified
             * INode id (or filename) within the specified directory.  Returns an
             * inode with type INodeType::INT_INVALID if it is unable to find
             * the specified child, or if the parent inode is invalid (i.e. not
             * a directory).
             */
            INode getChildOfDirectory(uint16_t parentid, uint16_t childid);
            INode getChildOfDirectory(uint16_t parentid, std::string filename);

            //! Sets a file's contents (replacing the current contents).
            FSResult::FSResult setFileContents(uint16_t id, const char *data, uint32_t len);

            //! Returns a file's contents.
            FSResult::FSResult getFileContents(uint16_t id, char **out, uint32_t * len_out, uint32_t len_max);

            //! Sets the length of a file (the dat_len and seg_len) fields, without actually
            //! adjusting the length of the file data or allocating new blocks (it only changes
            //! the field values).
            FSResult::FSResult setFileLengthDirect(uint32_t pos, uint32_t len);

            //! Sets the seg_next field for a FILE or SEGMENT block, without actually
            //! allocating a new block or validating the seg_next position.
            FSResult::FSResult setFileNextSegmentDirect(uint16_t id, uint32_t pos, uint32_t seg_next);

            //! This function returns the position of the next block for file data after the current block.
            uint32_t getFileNextBlock(uint16_t id, uint32_t pos);

            //! Erase a specified block, marking it as free in the free list.
            /*!
             * @note This simply erases BSIZE_FILE bytes from the specified
             *       position.  It does not check to make sure the position
             *       is actually the start of a block!
             */
            FSResult::FSResult resetBlock(uint32_t pos);

            //! Resolves a position in a file to a position in the disk image.
            uint32_t resolvePositionInFile(uint16_t inodeid, uint32_t pos);

            //! Resolve a pathname into an inode id.
            int32_t resolvePathnameToINodeID(std::string path);

            //! Sets the length of a file, allocating or erasing blocks / data where necessary.
            FSResult::FSResult truncateFile(uint16_t inodeid, uint32_t len);

            //! Allocates or frees enough blocks so that there is enough segment list blocks
            //! available to address all of the segments.
            /*!
             * @param pos The position of the file inode.
             * @param len The length of the data that needs to be addressed (i.e. size of the file).
             */
            FSResult::FSResult allocateInfoListBlocks(uint32_t pos, uint32_t len);

            //! Returns a FSFile object for interacting with the specified file at
            //! the specified inode.
            FSFile getFile(uint16_t inodeid);

            //! Retrieves the temporary block or creates a new one.
            /*!
             * Retrieves the temporary block or creates a new one if there is not one currently
             * located within the package (packages should have no more than one temporary block
             * at a single time).  The return value is the position to seek to, where you can then
             * read and write up to BSIZE_FILE - 4 bytes.
             *
             * @note The lookup / creation is *not* fast on large packages as it must search
             *       though all of the inodes in the package.  Therefore, this function makes use
             *       of a static variable to speed up subsequent calls to the function (the first
             *       call must still search through).  If you want to force the function to research
             *       the package for the temporary block (i.e. if you delete the temporary block),
             *       you can use the forceRecheck argument to do so.
             */
            uint32_t getTemporaryBlock(bool forceRecheck = false);

            //! Closes the filesystem.
            void close();

            //! Reserves an INode ID for future use without require the INode to actually be
            //! written to disk.
            void reserveINodeID(uint16_t id);

            //! Removes an INode ID reservation.
            void unreserveINodeID(uint16_t id);

            //! Update times on an inode.
            void updateTimes(uint16_t id, bool atime, bool mtime, bool ctime);

            //! Checks whether the specified position is valid.
            static LowLevel::FSResult::FSResult checkINodePositionIsValid(int pos);

            //! Copies the basename from basename into the filename.
            static LowLevel::FSResult::FSResult copyBasenameToFilename(const char* path,
                    char filename[256]);

        private:
            LowLevel::BlockStream * fd;
            LowLevel::FreeList * freelist;
            std::vector<uint16_t> reservedINodes;
        };
    }
}

#endif
