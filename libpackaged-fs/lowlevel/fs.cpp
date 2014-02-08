/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>

#include <string>
#include <iostream>
#include <fstream>
#include <libpackaged-fs/lowlevel/fs.h>
#include <libpackaged-fs/logging.h>
#include <libpackaged-fs/lowlevel/endian.h>
#include <libpackaged-fs/lowlevel/util.h>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <libpackaged-fs/lowlevel/freelist.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <vector>

namespace AppLib
{
    namespace LowLevel
    {
        FS::FS(LowLevel::BlockStream * fd)
        {
            if (fd == NULL)
                Logging::showDebugW("NULL file descriptor passed to FS constructor.");

            // Detect the endianness here (since we don't want the user to have to
            // remember to call it - stuff would break badly if they were required to
            // and forgot to call it).
            Endian::detectEndianness();

            this->fd = fd;
            this->freelist = new FreeList(this, fd);

#if 0 == 1
            // Check for text-mode stream, which will break binary packages.
            uint32_t tpos = this->getTemporaryBlock();
            if (tpos != 0)
            {
                char msg[5];
                char omsg[5];
                 msg[0] = 'm';
                 msg[1] = '\n';
                 msg[2] = 0;
                 msg[3] = 0;
                 msg[4] = 0;
                for (int i = 0; i < 5; i++)
                     omsg[i] = 0;
                 Util::seekp_ex(this->fd, tpos);
                 this->fd->write(&msg[0], 3);
                 this->fd->seekg(tpos + 2);
                 this->fd->read(&omsg[2], 3);
                 omsg[0] = 'm';
                 omsg[1] = '\n';
                if (strcmp(&omsg[0], &msg[0]) != 0)
                {
                    Logging::showErrorW("Text-mode stream passed to FS constructor.  Make sure");
                    Logging::showErrorO("the stream is in binary mode.");
                    this->fd = NULL;
                    return;
                }
            }
#endif
        }

        bool FS::isValid()
        {
            return (this->fd != NULL);
        }

        INode FS::getINodeByID(uint16_t id)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Retrieve the position using our getINodePositionByID
            // function.
            uint32_t ipos = this->getINodePositionByID(id);
            if (ipos == 0 || ipos < OFFSET_FSINFO)
                return INode(0, "", INodeType::INT_INVALID);
            return this->getINodeByPosition(ipos);
        }

        INode FS::getRealINodeByID(uint16_t id)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Retrieve the position using our getINodePositionByID
            // function.
            uint32_t ipos = this->getINodePositionByID(id);
            if (ipos == 0 || ipos < OFFSET_FSINFO)
                return INode(0, "", INodeType::INT_INVALID);
            return this->getINodeByRealPosition(ipos);
        }

        INode FS::getINodeByPosition(uint32_t ipos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());
            
            INode node = this->getINodeByRealPosition(ipos);
            
            // If this is a hardlink, resolve it automatically.
            if (node.type == INodeType::INT_HARDLINK)
                node = node.resolve(this);
            
            return node;
        }

        INode FS::getINodeByRealPosition(uint32_t ipos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            INode node(0, "", INodeType::INT_INVALID);

            // Seek to the inode position
            std::streampos old = this->fd->tellg();
            this->fd->seekg(ipos);

            // Read the data.
            Endian::doR(this->fd, reinterpret_cast < char *>(&node.inodeid), 2);
            Endian::doR(this->fd, reinterpret_cast < char *>(&node.type), 2);
            if (node.type == INodeType::INT_SEGINFO)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.info_next), 4);

                // Seek back to the original reading position.
                this->fd->seekg(old);

                return node;
            }
            else if (node.type == INodeType::INT_FREELIST)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.flst_next), 4);

                // Seek back to the original reading position.
                this->fd->seekg(old);

                return node;
            }
            else if (node.type == INodeType::INT_FSINFO)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.fs_name), 10);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.ver_major), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.ver_minor), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.ver_revision), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.app_name), 256);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.app_ver), 32);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.app_desc), 1024);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.app_author), 256);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.pos_root), 4);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.pos_freelist), 4);

                // Seek back to the original reading position.
                this->fd->seekg(old);

                return node;
            }
            Endian::doR(this->fd, reinterpret_cast < char *>(&node.filename), 256);
            if (node.type != INodeType::INT_HARDLINK)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.uid), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.gid), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.mask), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.atime), 8);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.mtime), 8);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.ctime), 8);
            }
            if (node.type == INodeType::INT_FILEINFO || node.type == INodeType::INT_SYMLINK || node.type == INodeType::INT_DEVICE)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.dev), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.rdev), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.nlink), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.blocks), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.dat_len), 4);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.info_next), 4);
            }
            else if (node.type == INodeType::INT_DIRECTORY)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.parent), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.children_count), 2);
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.children), DIRECTORY_CHILDREN_MAX * 2);
            }
            else if (node.type == INodeType::INT_HARDLINK)
                Endian::doR(this->fd, reinterpret_cast < char *>(&node.realid), 2);

            // Seek back to the original reading position.
            this->fd->seekg(old);
    
            // Ensure that if our node data is invalid, we return an invalid
            // INode instead of partial data.
            if (!node.verify())
                return INode(0, "", INodeType::INT_INVALID);

            return node;
        }

        FSResult::FSResult FS::writeINode(uint32_t pos, INode node)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Check to make sure the position is valid.
            FSResult::FSResult res = FS::checkINodePositionIsValid(pos);
            if (res != FSResult::E_SUCCESS)
                return res;

            // Check to make sure the inode ID is not already assigned.
            // TODO: This needs to be updated with a full list of inode types whose inode ID should
            //       be ignored.
            if (node.type != INodeType::INT_SEGINFO && node.type != INodeType::INT_FREELIST && this->getINodePositionByID(node.inodeid) != 0)
                return FSResult::E_FAILURE_INODE_ALREADY_ASSIGNED;

            // Do some sanity checks on the content.
            if (!node.verify())
                return FSResult::E_FAILURE_INODE_NOT_VALID;

            std::streampos old = this->fd->tellp();
            std::string data = node.getBinaryRepresentation();
            Util::seekp_ex(this->fd, pos);
            this->fd->write(data.c_str(), data.length());
            if (this->fd->fail())
                Logging::showErrorW("Write failure on write of new INode.");
            if (this->fd->bad())
                Logging::showErrorW("Write bad on write of new INode.");
            if (this->fd->eof())
                Logging::showErrorW("Write EOF on write of new INode.");
            // TODO: Should we return with failure if any of the above if
            // statements execute?

            const char *z = "";	// a const char* always has a \0 terminator, which we use to write into the file.
            // TODO: This needs to be updated with a full list of inode types.
            if (node.type == INodeType::INT_FILEINFO || node.type == INodeType::INT_SEGINFO || node.type == INodeType::INT_SYMLINK || node.type == INodeType::INT_FREELIST || node.type == INodeType::INT_DEVICE || node.type == INodeType::INT_HARDLINK)
            {
                for (int i = 0; i < BSIZE_FILE - data.length(); i += 1)
                    Endian::doW(this->fd, z, 1);
            }
            else if (node.type == INodeType::INT_DIRECTORY)
            {
                int target = BSIZE_DIRECTORY - data.length();
                for (int i = 0; i < target; i += 1)
                    Endian::doW(this->fd, z, 1);
            }
            else
                return FSResult::E_FAILURE_INODE_NOT_VALID;
            if (node.type == INodeType::INT_FILEINFO || node.type == INodeType::INT_SYMLINK || node.type == INodeType::INT_DIRECTORY || node.type == INodeType::INT_DEVICE || node.type == INodeType::INT_HARDLINK)
            {
                LowLevel::FSResult::FSResult sres = this->setINodePositionByID(node.inodeid, pos);
                if (sres != LowLevel::FSResult::E_SUCCESS)
                    return sres;
            }
            this->unreserveINodeID(node.inodeid);
            Util::seekp_ex(this->fd, old);
            return FSResult::E_SUCCESS;
        }

        FSResult::FSResult FS::updateRawINode(INode node, uint32_t pos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Check to make sure the position is valid.
            FSResult::FSResult res = FS::checkINodePositionIsValid(pos);
            if (res != FSResult::E_SUCCESS)
                return res;

            // Ensure that this INode is a type that allows updating via
            // manual positioning.
            if (node.type != INodeType::INT_FREELIST)
                return FSResult::E_FAILURE_INODE_NOT_VALID;

            // Do some sanity checks on the content.
            if (!node.verify())
                return FSResult::E_FAILURE_INODE_NOT_VALID;

            // Do a very simple update of the data.
            std::streampos old = this->fd->tellp();
            std::string data = node.getBinaryRepresentation();
            Util::seekp_ex(this->fd, pos);
            this->fd->write(data.c_str(), data.length());
            Util::seekp_ex(this->fd, old);
            return FSResult::E_SUCCESS;

        }

        FSResult::FSResult FS::updateINode(INode node)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Ensure that this INode is a type that can be updated.
            if (node.type != INodeType::INT_FILEINFO && node.type != INodeType::INT_DIRECTORY &&
                node.type != INodeType::INT_SYMLINK && node.type != INodeType::INT_DEVICE &&
                node.type != INodeType::INT_HARDLINK)
                return FSResult::E_FAILURE_INODE_NOT_VALID;

            // Check to make sure the inode ID is already assigned.
            Logging::showDebugW("Updating INode %i...", node.inodeid);
            uint32_t pos = this->getINodePositionByID(node.inodeid);
            if (pos == 0)
                return FSResult::E_FAILURE_INODE_NOT_ASSIGNED;

            // Check to make sure the position is valid.
            FSResult::FSResult res = FS::checkINodePositionIsValid(pos);
            if (res != FSResult::E_SUCCESS)
                return res;

            // Do some sanity checks on the content.
            if (!node.verify())
                return FSResult::E_FAILURE_INODE_NOT_VALID;

            std::streampos old = this->fd->tellp();
            std::string data = node.getBinaryRepresentation();
            Util::seekp_ex(this->fd, pos);
            this->fd->write(data.c_str(), data.length());
            // We do not write out the file data with zeros
            // as in writeINode because we want to keep the
            // content.
            LowLevel::FSResult::FSResult sres = this->setINodePositionByID(node.inodeid, pos);
            if (sres != LowLevel::FSResult::E_SUCCESS)
                return sres;
            Util::seekp_ex(this->fd, old);
            return FSResult::E_SUCCESS;
        }

        uint32_t FS::getINodePositionByID(uint16_t id)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            this->fd->clear();
            std::streampos old = this->fd->tellg();
            uint32_t newp = OFFSET_LOOKUP + (id * 4);
            this->fd->seekg(newp);
            uint32_t ipos = 0;
            Endian::doR(this->fd, reinterpret_cast < char *>(&ipos), 4);
            this->fd->seekg(old);
            return ipos;
        }

        uint16_t FS::getFirstFreeINodeNumber()
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            this->fd->clear();
            std::streampos old = this->fd->tellg();
            this->fd->seekg(OFFSET_LOOKUP);
            uint32_t ipos = 0;
            uint16_t count = 0;
            uint16_t ret = 0;
            Endian::doR(this->fd, reinterpret_cast < char *>(&ipos), 4);
            while ((ipos != 0 && count < 65535) || std::find(this->reservedINodes.begin(), this->reservedINodes.end(), count) != this->reservedINodes.end())
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&ipos), 4);
                count += 1;
            }
            if (count == 65535 && ipos != 0)
                ret = 0;
            else
                ret = count;
            this->fd->seekg(old);
            return ret;
        }

        FSResult::FSResult FS::setINodePositionByID(uint16_t id, uint32_t pos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            if (pos != 0)
            {
                // Check to make sure the position is valid.
                FSResult::FSResult res = FS::checkINodePositionIsValid(pos);
                if (res != FSResult::E_SUCCESS)
                    return res;
            }

            std::streampos old = this->fd->tellp();
            Util::seekp_ex(this->fd, OFFSET_LOOKUP + (id * 4));
            Endian::doW(this->fd, reinterpret_cast < char *>(&pos), 4);
            Util::seekp_ex(this->fd, old);
            return FSResult::E_SUCCESS;
        }

        uint32_t FS::getFirstFreeBlock(INodeType::INodeType type)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Use the FreeList class to return a new free block.
            return this->freelist->allocateBlock();
        }

        bool FS::isBlockFree(uint32_t pos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Use the FreeList class to test whether the block is free.
            return this->freelist->isBlockFree(pos);
        }

        FSResult::FSResult FS::addChildToDirectoryINode(uint16_t parentid, uint16_t childid)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            if (parentid == childid)
                return FSResult::E_FAILURE_GENERAL;

            signed int type_offset = 2;
            signed int children_count_offset = 292;
            signed int children_offset = 294;
            uint32_t pos = this->getINodePositionByID(parentid);
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Read to make sure it's a directory.
            uint16_t parent_type = (uint16_t) INodeType::INT_UNSET;
            this->fd->seekg(pos + type_offset);
            Endian::doR(this->fd, reinterpret_cast < char *>(&parent_type), 2);
            if (parent_type != INodeType::INT_DIRECTORY)
            {
                this->fd->seekg(oldg);
                Util::seekp_ex(this->fd, oldp);
                return FSResult::E_FAILURE_NOT_A_DIRECTORY;
            }

            // Now seek to the start of the list of directory children.
            this->fd->seekg(pos + children_offset);
            Util::seekp_ex(this->fd, pos + children_offset);

            // Find the first available child slot.
            uint16_t ccinode = 0;
            uint16_t count = 0;
            Endian::doR(this->fd, reinterpret_cast < char *>(&ccinode), 2);
            while (ccinode != 0 && count < DIRECTORY_CHILDREN_MAX - 1)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&ccinode), 2);
                count += 1;
            }
            if (count == DIRECTORY_CHILDREN_MAX - 1 && ccinode != 0)
            {
                this->fd->seekg(oldg);
                Util::seekp_ex(this->fd, oldp);
                return FSResult::E_FAILURE_MAXIMUM_CHILDREN_REACHED;
            }
            else
            {
                uint32_t writeg = this->fd->tellg();
                Util::seekp_ex(this->fd, writeg - 2);
                Endian::doW(this->fd, reinterpret_cast < char *>(&childid), 2);

                uint16_t children_count_current = 0;
                this->fd->seekg(pos + children_count_offset);
                Endian::doR(this->fd, reinterpret_cast < char *>(&children_count_current), 2);
                children_count_current += 1;
                Util::seekp_ex(this->fd, pos + children_count_offset);
                Endian::doW(this->fd, reinterpret_cast < char *>(&children_count_current), 2);
                this->fd->seekg(oldg);
                Util::seekp_ex(this->fd, oldp);

                // Update times.
                this->updateTimes(parentid, false, true, true);

                return FSResult::E_SUCCESS;
            }
        }

        FSResult::FSResult FS::removeChildFromDirectoryINode(uint16_t parentid, uint16_t childid)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            signed int type_offset = 2;
            signed int children_count_offset = 292;
            signed int children_offset = 294;
            uint32_t pos = this->getINodePositionByID(parentid);
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Read to make sure it's a directory.
            uint16_t parent_type = (uint16_t) INodeType::INT_UNSET;
            this->fd->seekg(pos + type_offset);
            Endian::doR(this->fd, reinterpret_cast < char *>(&parent_type), 2);
            if (parent_type != INodeType::INT_DIRECTORY)
            {
                this->fd->seekg(oldg);
                Util::seekp_ex(this->fd, oldp);
                return FSResult::E_FAILURE_NOT_A_DIRECTORY;
            }

            // Now seek to the start of the list of directory children.
            this->fd->seekg(pos + children_offset);
            Util::seekp_ex(this->fd, pos + children_offset);

            // Find the slot that the child inode is in.
            uint16_t ccinode = 0;
            uint16_t count = 0;
            Endian::doR(this->fd, reinterpret_cast < char *>(&ccinode), 2);
            while (ccinode != childid && count < DIRECTORY_CHILDREN_MAX - 1)
            {
                Endian::doR(this->fd, reinterpret_cast < char *>(&ccinode), 2);
                count += 1;
            }
            if (count == DIRECTORY_CHILDREN_MAX - 1 && ccinode != childid)
            {
                this->fd->seekg(oldg);
                Util::seekp_ex(this->fd, oldp);
                return FSResult::E_FAILURE_INVALID_FILENAME;
            }
            else
            {
                uint32_t writeg = this->fd->tellg();
                Util::seekp_ex(this->fd, writeg - 2);
                uint16_t zeroid = 0;
                Endian::doW(this->fd, reinterpret_cast < char *>(&zeroid), 2);

                uint16_t children_count_current = 0;
                this->fd->seekg(pos + children_count_offset);
                Endian::doR(this->fd, reinterpret_cast < char *>(&children_count_current), 2);
                children_count_current -= 1;
                Util::seekp_ex(this->fd, pos + children_count_offset);
                Endian::doW(this->fd, reinterpret_cast < char *>(&children_count_current), 2);
                this->fd->seekg(oldg);
                Util::seekp_ex(this->fd, oldp);

                // Update times.
                this->updateTimes(parentid, false, true, true);

                return FSResult::E_SUCCESS;
            }
        }

        FSResult::FSResult FS::filenameIsUnique(uint16_t parentid, std::string filename)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            std::vector < INode > inodechildren = this->getChildrenOfDirectory(parentid);
            for (unsigned int i = 0; i < inodechildren.size(); i += 1)
            {
                if (filename == inodechildren[i].filename)
                {
                    return FSResult::E_FAILURE_NOT_UNIQUE;
                }
            }
            return FSResult::E_SUCCESS;	// Indicates unique.
        }

        std::vector < INode > FS::getChildrenOfDirectory(uint16_t parentid)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            std::vector < INode > inodechildren;
            INode node = this->getINodeByID(parentid);
            if (node.type != INodeType::INT_DIRECTORY)
            {
                return inodechildren;
            }

            uint16_t children_looped = 0;
            uint16_t total_looped = 0;
            while (children_looped < node.children_count && total_looped < DIRECTORY_CHILDREN_MAX)
            {
                uint16_t cinode = node.children[total_looped];
                total_looped += 1;
                if (cinode == 0)
                {
                    continue;
                }
                else
                {
                    children_looped += 1;
                    INode cnode = this->getINodeByID(cinode);
                    if (cnode.type == INodeType::INT_FILEINFO || cnode.type == INodeType::INT_DIRECTORY || cnode.type == INodeType::INT_SYMLINK || cnode.type == INodeType::INT_DEVICE || cnode.type == INodeType::INT_HARDLINK)
                    {
                        inodechildren.insert(inodechildren.end(), cnode);
                    }
                }
            }

            return inodechildren;
        }

        INode FS::getChildOfDirectory(uint16_t parentid, uint16_t childid)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            INode node = this->getINodeByID(parentid);
            if (node.type == INodeType::INT_INVALID)
            {
                return INode(0, "", INodeType::INT_INVALID);
            }

            uint16_t children_looped = 0;
            uint16_t total_looped = 0;
            while (children_looped < node.children_count && total_looped < DIRECTORY_CHILDREN_MAX)
            {
                uint16_t cinode = node.children[total_looped];
                total_looped += 1;
                if (cinode == 0)
                {
                    continue;
                }
                else
                {
                    children_looped += 1;
                    INode cnode = this->getINodeByID(cinode);
                    if (cnode.type == INodeType::INT_FILEINFO || cnode.type == INodeType::INT_DIRECTORY || cnode.type == INodeType::INT_SYMLINK || cnode.type == INodeType::INT_DEVICE || cnode.type == INodeType::INT_HARDLINK)
                    {
                        if (cnode.inodeid == childid)
                        {
                            return cnode;
                        }
                    }
                }
            }

            return INode(0, "", INodeType::INT_INVALID);
        }

        INode FS::getChildOfDirectory(uint16_t parentid, std::string filename)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            INode node = this->getINodeByID(parentid);
            if (node.type == INodeType::INT_INVALID)
                return INode(0, "", INodeType::INT_INVALID);

            uint16_t children_looped = 0;
            uint16_t total_looped = 0;
            while (children_looped < node.children_count && total_looped < DIRECTORY_CHILDREN_MAX)
            {
                uint16_t cinode = node.children[total_looped];
                total_looped += 1;
                if (cinode == 0)
                {
                    continue;
                }
                else
                {
                    children_looped += 1;
                    INode cnode = this->getINodeByID(cinode);
                    if (cnode.type == INodeType::INT_FILEINFO || cnode.type == INodeType::INT_DIRECTORY || cnode.type == INodeType::INT_SYMLINK || cnode.type == INodeType::INT_DEVICE || cnode.type == INodeType::INT_HARDLINK)
                        if (filename == cnode.filename)
                            return cnode;
                }
            }

            return INode(0, "", INodeType::INT_INVALID);
        }

        FSResult::FSResult FS::setFileContents(uint16_t id, const char *data, uint32_t len)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Our new version of this function is simply going to use
            // the FSFile class.
            FSFile f = this->getFile(id);
            f.truncate(len);
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.open();
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.seekp(0);
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.write(data, len);
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.close();
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;

            return FSResult::E_SUCCESS;
        }

        FSResult::FSResult FS::getFileContents(uint16_t id, char **data_out, uint32_t * len_out, uint32_t len_max)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Our new version of this function is simply going to use
            // the FSFile class.
            FSFile f = this->getFile(id);
            uint32_t fsize = f.size();
            *len_out = (fsize < len_max) ? fsize : len_max;
            *data_out = (char *) malloc(*len_out);

            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.open();
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.seekg(0);
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.read(*data_out, *len_out);
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;
            f.close();
            if (f.fail())
                return FSResult::E_FAILURE_GENERAL;

            return FSResult::E_SUCCESS;
        }

        FSResult::FSResult FS::setFileLengthDirect(uint32_t pos, uint32_t len)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            signed int file_blocks_offset = 296;
            signed int file_len_offset = 298;

            this->fd->clear();

            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Get the type directly.
            uint16_t type_raw = (uint16_t) INodeType::INT_INVALID;
            this->fd->seekg(pos + 2);
            Endian::doR(this->fd, reinterpret_cast < char *>(&type_raw), 2);

            if (type_raw == INodeType::INT_FILEINFO || type_raw == INodeType::INT_SYMLINK)
            {
                Util::seekp_ex(this->fd, pos + file_len_offset);
                Endian::doW(this->fd, reinterpret_cast < char *>(&len), 4);
                Util::seekp_ex(this->fd, pos + file_blocks_offset);
                uint16_t blocks = ceil(len / (double) BSIZE_FILE);
                Endian::doW(this->fd, reinterpret_cast < char *>(&blocks), 2);
                Util::seekp_ex(this->fd, oldp);
                this->fd->seekg(oldg);
                this->fd->seekp(oldp);
                return FSResult::E_SUCCESS;
            }
            else
            {
                this->fd->seekg(oldg);
                return FSResult::E_FAILURE_INVALID_POSITION;
            }
        }

        FSResult::FSResult FS::setFileNextSegmentDirect(uint16_t id, uint32_t pos, uint32_t seg_next)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            signed int file_info_next_offset = 302;

            // Store the current positions.
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Get the base position of the specified inode.
            uint32_t bpos = this->getINodePositionByID(id);
            INode node = this->getINodeByPosition(bpos);
            if (node.type != INodeType::INT_FILEINFO && node.type != INodeType::INT_SYMLINK)
            {
                return FSResult::E_FAILURE_NOT_A_FILE;
            }
            if (bpos == pos)
            {
                // We're setting the position of the first segment
                // in the file.
                std::streampos oldp = this->fd->tellp();
                this->fd->seekg(bpos + file_info_next_offset);
                Endian::doW(this->fd, reinterpret_cast < char *>(&seg_next), 4);
                this->fd->seekp(oldp);
                this->fd->seekg(oldg);
                return FSResult::E_SUCCESS;
            }

            // Now loop through all of the segment positions.
            bool gnext = false;
            uint32_t spos = 0;
            uint32_t ipos = bpos;
            uint32_t hsize = HSIZE_FILE;
            while (ipos != 0)
            {
                for (int i = hsize; i < BSIZE_FILE; i += 4)
                {
                    this->fd->seekg(bpos + i);
                    spos = 0;
                    Endian::doR(this->fd, reinterpret_cast < char *>(&spos), 4);
                    if (spos == 0)
                    {
                        // End of segment list.  Return 0.
                        return FSResult::E_FAILURE_INODE_NOT_ASSIGNED;
                    }
                    else if (spos == pos)
                    {
                        // Matched, we need to fetch the next one.
                        gnext = true;
                    }
                    else if (gnext)
                    {
                        // Replace the segment value.
                        std::streampos oldp = this->fd->tellp();
                        this->fd->seekp(bpos + i);
                        Endian::doW(this->fd, reinterpret_cast < char *>(&seg_next), 4);
                        this->fd->seekp(oldp);
                        this->fd->seekg(oldg);
                        return FSResult::E_SUCCESS;
                    }
                }
                hsize = HSIZE_SEGINFO;
                INode inode = this->getINodeByPosition(ipos);
                if (inode.type != INodeType::INT_SEGINFO)
                    break;
                ipos = inode.info_next;
            }

            // Unable to locate the current segment within the
            // specified file ID.
            this->fd->seekg(oldg);
            return FSResult::E_FAILURE_INODE_NOT_ASSIGNED;
        }

        uint32_t FS::getFileNextBlock(uint16_t id, uint32_t pos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Store the current positions.
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Get the base position of the specified inode.
            uint32_t bpos = this->getINodePositionByID(id);

            // Now loop through all of the segment positions.
            bool gnext = false;
            uint32_t spos = 0;
            uint32_t ipos = bpos;
            uint32_t hsize = HSIZE_FILE;
            while (ipos != 0)
            {
                for (int i = hsize; i < BSIZE_FILE; i += 4)
                {
                    this->fd->seekg(bpos + i);
                    spos = 0;
                    Endian::doR(this->fd, reinterpret_cast < char *>(&spos), 4);
                    if (spos == 0)
                    {
                        // End of segment list.  Return 0.
                        this->fd->seekg(oldg);
                        return 0;
                    }
                    else if (spos == pos)
                    {
                        // Matched, we need to fetch the next one.
                        gnext = true;
                    }
                    else if (gnext)
                    {
                        // Return the next segment value.
                        this->fd->seekg(oldg);
                        return spos;
                    }
                }
                hsize = HSIZE_SEGINFO;
                INode inode = this->getINodeByPosition(ipos);
                if (inode.type != INodeType::INT_SEGINFO)
                    break;
                ipos = inode.info_next;
            }

            // Unable to locate the current segment within the
            // specified file ID.
            this->fd->seekg(oldg);
            return 0;
        }

        FSResult::FSResult FS::resetBlock(uint32_t pos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            if (this->freelist->isBlockFree(pos) || pos % 4096 != 0)
            {
                return FSResult::E_FAILURE_INODE_NOT_VALID;
            }

            /*
             * std::streampos oldp = this->fd->tellp();
             * Util::seekp_ex(this->fd, pos);
             * const char * zero = "\0";
             * for (int i = 0; i < BSIZE_FILE; i += 1)
             * {
             * this->fd->write(zero, 1);
             * }
             * Util::seekp_ex(this->fd, oldp);
             */

            // Block must be marked as unused through the
            // free list allocation class.
            this->freelist->freeBlock(pos);

            return FSResult::E_SUCCESS;
        }

        uint32_t FS::resolvePositionInFile(uint16_t inodeid, uint32_t pos)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Store the current positions.
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Get the base position of the specified inode.
            uint32_t bpos = this->getINodePositionByID(inodeid);

            // Now loop through all of the segment positions.
            uint32_t bcount = 0;
            uint32_t spos = 0;
            uint32_t ipos = bpos;
            uint32_t hsize = HSIZE_FILE;
            while (ipos != 0)
            {
                for (int i = hsize; i < BSIZE_FILE; i += 4)
                {
                    this->fd->seekg(bpos + i);
                    spos = 0;
                    Endian::doR(this->fd, reinterpret_cast < char *>(&spos), 4);
                    if (spos == 0)
                    {
                        // Invalid segment position (i.e. there are
                        // no more segments available).
                        this->fd->seekg(oldg);
                        return 0;
                    }

                    if (bcount < pos && bcount + 4096 < pos)
                    {
                        // We haven't reached our target block yet.
                        bcount += 4096;
                        continue;
                    }
                    else if (bcount < pos && bcount + 4096 > pos)
                    {
                        // This is our target block.
                        this->fd->seekg(oldg);
                        return spos + (pos - bcount);
                    }
                    else
                    {
                        // We're past our target block.
                        this->fd->seekg(oldg);
                        return 0;
                    }
                }
                hsize = HSIZE_SEGINFO;
                INode inode = this->getINodeByPosition(ipos);
                if (inode.type != INodeType::INT_SEGINFO)
                    break;
                ipos = inode.info_next;
            }

            // Unable to locate the position within the file.
            this->fd->seekg(oldg);
            return 0;
        }

        int32_t FS::resolvePathnameToINodeID(std::string path)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            std::vector < std::string > components;
            std::string buf = "";
            for (int i = 0; i < path.length(); i += 1)
            {
                if (path[i] == '/')
                {
                    if (buf != "")
                    {
                        components.insert(components.end(), buf);
                        buf = "";
                    }
                }
                else
                {
                    buf += path[i];
                }
            }
            if (buf.length() > 0)
            {
                components.insert(components.end(), buf);
                buf = "";
            }

            uint16_t id = 0;
            for (int i = 0; i < components.size(); i += 1)
            {
                if (components[i] == ".")
                    id = id;
                else if (components[i] == "..")
                {
                    INode node = this->getINodeByID(id);
                    if (node.type == INodeType::INT_INVALID)
                        return -ENOENT;
                    id = node.parent;
                }
                else
                {
                    INode node = this->getChildOfDirectory(id, components[i].c_str());
                    if (node.type == INodeType::INT_INVALID)
                        return -ENOENT;
                    id = node.inodeid;
                }
            }
            return id;
        }

        FSResult::FSResult FS::truncateFile(uint16_t inodeid, uint32_t len)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Store the current positions.
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Get the base position of the specified inode.
            uint32_t bpos = this->getINodePositionByID(inodeid);

            // Then get the INode and find out the data length.
            INode node = this->getINodeByPosition(bpos);
            if (node.type != INodeType::INT_FILEINFO &&
                node.type != INodeType::INT_SYMLINK)
                return FSResult::E_FAILURE_INODE_NOT_VALID;

            if (node.dat_len == len)
                return FSResult::E_SUCCESS;
            else if (node.dat_len > len)
            {
                // We need to delete blocks at the end of the file.
                // Calculate the number of blocks to delete.
                uint16_t blocks_to_delete = node.blocks - ceil(len / (double) BSIZE_FILE);

                // Now loop through all of the segment positions.
                uint32_t bcount = 0;
                uint32_t spos = 0;
                uint32_t ipos = bpos;
                uint32_t hsize = HSIZE_FILE;
                while (ipos != 0)
                {
                    for (int i = hsize; i < BSIZE_FILE; i += 4)
                    {
                        this->fd->seekg(bpos + i);
                        spos = 0;
                        Endian::doR(this->fd, reinterpret_cast < char *>(&spos), 4);
                        if (spos == 0)
                        {
                            // We've run out of segments to erase.
                            ipos = 0;	// Make it jump out of the while() loop.
                            break;
                        }

                        if (bcount >= node.blocks - blocks_to_delete)
                        {
                            // First remove the block from the file segment list.
                            uint32_t zeropos = 0;
                            this->fd->seekp(bpos + i);
                            Endian::doW(this->fd, reinterpret_cast < char *>(&zeropos), 4);

                            // Next use resetBlock to erase the data in it (this also
                            // frees it in the FreeList).
                            this->resetBlock(spos);
                        }

                        bcount += 1;
                    }
                    if (ipos != 0)
                    {
                        hsize = HSIZE_SEGINFO;
                        INode inode = this->getINodeByPosition(ipos);
                        if (inode.type != INodeType::INT_SEGINFO)
                        {
                            this->fd->seekg(oldg);
                            this->fd->seekp(oldp);
                            return FSResult::E_FAILURE_INODE_NOT_VALID;
                        }
                        ipos = inode.info_next;
                    }
                }

                // Now set the file's data length.
                FSResult::FSResult res = this->setFileLengthDirect(bpos, len);
                if (res != FSResult::E_SUCCESS)
                    return res;

                // Now free up any segment list blocks.
                res = this->allocateInfoListBlocks(bpos, len);
                if (res != FSResult::E_SUCCESS)
                    return res;

                // We successfully truncated the file.
                this->fd->seekg(oldg);
                this->fd->seekp(oldp);
                return FSResult::E_SUCCESS;
            }
            else if (node.dat_len < len)
            {
                // Allocate new segment list blocks before we
                // attempt to cycle through / store data in them.
                FSResult::FSResult res = this->allocateInfoListBlocks(bpos, len);
                if (res != FSResult::E_SUCCESS)
                    return res;

                // We need to add blocks at the end of the file.
                // Calculate the number of blocks to add.
                uint16_t blocks_to_add = ceil(len / (double) BSIZE_FILE) - node.blocks;

                // Now loop through all of the segment positions.
                uint32_t bcount = 0;
                uint32_t spos = 0;
                uint32_t ipos = bpos;
                uint32_t hsize = HSIZE_FILE;
                while (ipos != 0)
                {
                    for (int i = hsize; i < BSIZE_FILE; i += 4)
                    {
                        this->fd->seekg(bpos + i);
                        spos = 0;
                        Endian::doR(this->fd, reinterpret_cast < char *>(&spos), 4);
                        if (spos != 0)
                        {
                            // We don't want to touch this position since it already
                            // points to an existing segment.
                            continue;
                        }

                        if (bcount < blocks_to_add)
                        {
                            // Allocate a new block.
                            uint32_t npos = this->freelist->allocateBlock();

                            // Now add it to the file segment list.
                            this->fd->seekp(bpos + i);
                            Endian::doW(this->fd, reinterpret_cast < char *>(&npos), 4);
                        }
                        else
                        {
                            ipos = 0;
                            break;
                        }

                        bcount += 1;
                    }
                    if (ipos != 0)
                    {
                        hsize = HSIZE_SEGINFO;
                        INode inode = this->getINodeByPosition(ipos);
                        if (inode.type != INodeType::INT_SEGINFO)
                        {
                            this->fd->seekg(oldg);
                            this->fd->seekp(oldp);
                            return FSResult::E_FAILURE_INODE_NOT_VALID;
                        }
                        ipos = inode.info_next;
                    }
                }

                // Now set the file's data length.
                res = this->setFileLengthDirect(bpos, len);
                if (res != FSResult::E_SUCCESS)
                    return res;

                // We successfully truncated the file.
                this->fd->seekg(oldg);
                this->fd->seekp(oldp);
                return FSResult::E_SUCCESS;
            }

            // Impossible to get here?
            return FSResult::E_FAILURE_UNKNOWN;
        }

        FSResult::FSResult FS::allocateInfoListBlocks(uint32_t pos, uint32_t len)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            signed int file_info_next_offset = 302;
            signed int info_info_next_offset = 4;
            signed int segments_in_file_block = (BSIZE_FILE - HSIZE_FILE) / 4;
            signed int segments_in_info_block = (BSIZE_FILE - HSIZE_SEGINFO) / 4;

            // Store the current positions.
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Get the INode.
            INode node = this->getINodeByPosition(pos);
            // TODO: Verify type of INode.

            // First calculate the number of segment info 'markers' we need to
            // address data in the entire file.
            uint32_t mcount = len / BSIZE_FILE;

            // Subtract the number that can be addressed in the file block as we're
            // only interested in the number of additional blocks.
            if (mcount > segments_in_file_block)
                mcount -= segments_in_file_block;
            else
                mcount = 0;

            // Do the same for the current number of segment allocated.
            uint32_t ccount = node.blocks;
            if (ccount > segments_in_file_block)
                ccount -= segments_in_file_block;
            else
                ccount = 0;

            // Calculate how many *additional* info list blocks we'd need to
            // index all of the file, as well as how many info blocks are
            // currently being used.
            uint32_t tilcount = 0;
            uint32_t cilcount = 0;
            for (uint32_t i = 0; i < mcount; i += segments_in_info_block)
                tilcount += 1;
            for (uint32_t i = 0; i < ccount; i += segments_in_file_block)
                cilcount += 1;

            // Determine whether we need to increase, decrease or leave the
            // number of allocated blocks the same.
            if (tilcount == cilcount)
                return FSResult::E_SUCCESS;
            else if (tilcount < cilcount)
            {
                // Free up some blocks.  First loop through and build
                // a vector list of all of the positions of the info
                // list blocks (as there is no way to reverse through
                // the list using I/O).
                std::vector < uint32_t > list_positions;
                uint32_t lpos = 0;
                this->fd->seekg(pos + file_info_next_offset);
                Endian::doR(this->fd, reinterpret_cast < char *>(&lpos), 4);
                while (lpos != 0)
                {
                    list_positions.insert(list_positions.begin(), lpos);
                    this->fd->seekg(lpos + info_info_next_offset);
                    Endian::doR(this->fd, reinterpret_cast < char *>(&lpos), 4);
                }

                // Now delete the info list blocks.
                while (tilcount < cilcount)
                {
                    uint32_t dpos = list_positions[list_positions.size() - 1];
                    uint32_t ppos = 0;
                    uint32_t poff = 0;
                    if (list_positions.size() == 1)
                    {
                        ppos = pos;
                        poff = file_info_next_offset;
                    }
                    else
                    {
                        ppos = list_positions[list_positions.size() - 2];
                        poff = info_info_next_offset;
                    }

                    // Erase the link from the previous info block to this one.
                    this->fd->seekp(ppos + poff);
                    uint32_t zeropos = 0;
                    Endian::doW(this->fd, reinterpret_cast < char *>(&zeropos), 4);

                    // Now erase the block.
                    this->resetBlock(dpos);

                    cilcount -= 1;
                }

                return FSResult::E_SUCCESS;
            }
            else if (tilcount > cilcount)
            {
                // Allocate some blocks.  Fetch the position of the last
                // allocated block.
                uint32_t lpos = 0;
                uint32_t ppos = 0;
                this->fd->seekg(pos + file_info_next_offset);
                Endian::doR(this->fd, reinterpret_cast < char *>(&lpos), 4);
                while (lpos != 0)
                {
                    ppos = lpos;
                    this->fd->seekg(lpos + info_info_next_offset);
                    Endian::doR(this->fd, reinterpret_cast < char *>(&lpos), 4);
                }

                // Now allocate as many blocks as we need.
                while (tilcount > cilcount)
                {
                    // Get a new block.
                    uint32_t npos = this->freelist->allocateBlock();

                    // Set a link from the previous block to the new one.
                    uint32_t poff = 0;
                    if (ppos == 0)
                    {
                        ppos = pos;
                        poff = file_info_next_offset;
                    }
                    else
                        poff = info_info_next_offset;

                    this->fd->seekp(ppos + poff);
                    Endian::doW(this->fd, reinterpret_cast < char *>(&npos), 4);

                    ppos = npos;
                    cilcount += 1;
                }

                return FSResult::E_SUCCESS;
            }

            return FSResult::E_FAILURE_UNKNOWN;
        }

        FSFile FS::getFile(uint16_t inodeid)
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            return FSFile(this, this->fd, inodeid);
        }

        uint32_t FS::getTemporaryBlock(bool forceRecheck)
        {
            // FIXME: This function is most certainly broken and needs
            //        rewriting before use.
            assert(false);
            
            // We use a static variable to speed up later calls.
            static uint32_t temporary_position = 0;
            if (temporary_position != 0 && !forceRecheck)
                return temporary_position;

            uint32_t block_position = OFFSET_DATA + 2;
            std::streampos oldg = this->fd->tellg();
            std::streampos oldp = this->fd->tellp();

            // Run through each of the blocks and check to see whether
            // they are a temporary block or not.
            this->fd->seekg(block_position);
            uint16_t type_stor = INodeType::INT_UNSET;
            uint32_t cpos = this->fd->tellg();
            while (!this->fd->eof() && cpos == block_position && !this->freelist->isBlockFree(cpos))
            {
                // FIXME: What the hell is this doing??!?!
                Endian::doR(this->fd, reinterpret_cast < char *>(&type_stor), 2);
                switch (type_stor)
                {
                    case INodeType::INT_DIRECTORY:
                        block_position += BSIZE_DIRECTORY;
                        this->fd->seekg(block_position);
                        break;
                    case INodeType::INT_FILEINFO:
                    case INodeType::INT_SEGINFO:
                        block_position += BSIZE_FILE;
                        this->fd->seekg(block_position);
                        break;
                    case INodeType::INT_INVALID:
                        this->fd->seekg(oldg);
                        return 0;
                    case INodeType::INT_TEMPORARY:
                        this->fd->seekg(oldg);
                        temporary_position = block_position + 2;
                        return block_position + 2;
                    case INodeType::INT_UNSET:
                        break;
                    default:
                        this->fd->seekg(oldg);
                        return 0;
                }
            }
            this->fd->seekg(oldg);

            // No temporary block allocated; allocate a new one.
            uint32_t newpos = this->getFirstFreeBlock(INodeType::INT_FILEINFO);
            if (newpos == 0)
                return 0;
            Util::seekp_ex(this->fd, newpos);
            uint16_t zeroid = 0;
            uint16_t tempid = INodeType::INT_TEMPORARY;
            Endian::doW(this->fd, reinterpret_cast < char *>(&zeroid), 2);
            Endian::doW(this->fd, reinterpret_cast < char *>(&tempid), 2);
            Util::seekp_ex(this->fd, oldp);
            newpos += 4;
            temporary_position = newpos;
            return newpos;
        }

        void FS::close()
        {
            assert( /* Check the stream is not in text-mode. */ this->isValid());

            // Close the file stream.
            this->fd->close();
        }

        void FS::reserveINodeID(uint16_t id)
        {
            this->reservedINodes.insert(this->reservedINodes.end(), id);
        }

        void FS::unreserveINodeID(uint16_t id)
        {
            std::vector<uint16_t>::iterator it =
                std::find(this->reservedINodes.begin(), this->reservedINodes.end(), id);
            if (it != this->reservedINodes.end())
                this->reservedINodes.erase(it);
        }

        LowLevel::FSResult::FSResult FS::checkINodePositionIsValid(int pos)
        {
            if (pos < OFFSET_DATA || (pos - OFFSET_DATA) % 4096 != 0)
                return LowLevel::FSResult::E_FAILURE_INVALID_POSITION;
            return LowLevel::FSResult::E_SUCCESS;
        }

        LowLevel::FSResult::FSResult copyBasenameToFilename(const char* path, char filename[256])
        {
            std::string basename = Util::extractBasenameFromPath(path);
            if (basename.length() >= 256)
                return AppLib::LowLevel::FSResult::E_FAILURE_INVALID_FILENAME;
            for (int i = 0; i < 256; i += 1)
                filename[i] = 0;
            for (int i = 0; i < 255; i += 1)
            {
                if (basename[i] == 0)
                    break;
                filename[i] = basename[i];
            }
            return AppLib::LowLevel::FSResult::E_SUCCESS;
        }

        void FS::updateTimes(uint16_t id, bool atime, bool mtime, bool ctime)
        {
            INode node = this->getINodeByID(id);
            if (atime)
                node.atime = APPFS_TIME();
            if (mtime)
                node.mtime = APPFS_TIME();
            if (ctime)
                node.ctime = APPFS_TIME();
            this->updateINode(node);
        }
    }
}
