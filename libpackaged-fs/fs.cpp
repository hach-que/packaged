/* vim: set ts=4 sw=4 tw=0 :*/

#include <exception>
#include <cstdlib>
#include <libpackaged-fs/fs.h>
#include <libpackaged-fs/exception/package.h>
#include <libpackaged-fs/lowlevel/util.h>
#include <linux/kdev_t.h>

namespace AppLib
{
    FS::FS(std::string path, uid_t uid, gid_t gid)
        : uid(uid), gid(gid)
    {
        this->stream = new LowLevel::BlockStream(path.c_str());
        if (!this->stream->is_open())
        {
            delete this->stream;
            throw Exception::PackageNotFound();
        }
        this->filesystem = new LowLevel::FS(this->stream);
        if (!this->filesystem->isValid())
        {
            this->stream->close();
            delete this->stream;
            delete this->filesystem;
            throw Exception::PackageNotValid();
        }
    }

    void FS::getattr(std::string path, struct stat& stbufOut) const
    {
        LowLevel::INode buf;
        if (!this->retrievePathToINode(path, buf))
            throw Exception::FileNotFound();

        // Ensure that the inode is also one of the
        // accepted types.
        if (buf.type != LowLevel::INodeType::INT_DIRECTORY &&
                buf.type != LowLevel::INodeType::INT_FILEINFO &&
                buf.type != LowLevel::INodeType::INT_SYMLINK &&
                buf.type != LowLevel::INodeType::INT_DEVICE &&
                buf.type != LowLevel::INodeType::INT_HARDLINK)
            throw Exception::FileNotFound();

        // Resolve hardlink if needed.
        if (buf.type == LowLevel::INodeType::INT_HARDLINK)
            buf = buf.resolve(this->filesystem);

        // Set the values into the stat structure.
        stbufOut.st_ino = buf.inodeid;
        stbufOut.st_dev = buf.dev;
        stbufOut.st_mode = buf.mask;
        stbufOut.st_nlink = buf.nlink;
        stbufOut.st_uid = buf.uid;
        stbufOut.st_gid = buf.gid;
        stbufOut.st_rdev = buf.rdev;
        stbufOut.st_atime = buf.atime;
        stbufOut.st_mtime = buf.mtime;
        stbufOut.st_ctime = buf.ctime;

        // File-based inodes are treated differently to directory inodes.
        if (buf.type == LowLevel::INodeType::INT_FILEINFO ||
                buf.type == LowLevel::INodeType::INT_SYMLINK ||
                buf.type == LowLevel::INodeType::INT_DEVICE)
        {
            stbufOut.st_size = buf.dat_len;
            stbufOut.st_blksize = BSIZE_FILE;
            stbufOut.st_blocks = buf.blocks;

            if (buf.type == LowLevel::INodeType::INT_FILEINFO)
                stbufOut.st_mode = S_IFREG | stbufOut.st_mode;
            else if (buf.type == LowLevel::INodeType::INT_SYMLINK)
                stbufOut.st_mode = S_IFLNK | stbufOut.st_mode;
        }
        else if (buf.type == LowLevel::INodeType::INT_DIRECTORY)
        {
            stbufOut.st_size = BSIZE_DIRECTORY;
            stbufOut.st_blksize = BSIZE_FILE;
            stbufOut.st_blocks = BSIZE_DIRECTORY / BSIZE_FILE;
            stbufOut.st_mode = S_IFDIR | stbufOut.st_mode;
        }
        else
            throw Exception::InternalInconsistency();
    }

    std::string FS::readlink(std::string path) const
    {
        LowLevel::INode buf;
        if (!this->retrievePathToINode(path, buf))
            throw Exception::FileNotFound();

        if (buf.type == LowLevel::INodeType::INT_SYMLINK)
        {
            // Read the link information out of the file.
            FSFile* file = new FSFile(this->filesystem, this->stream, buf.inodeid);
            file->open(std::ios_base::in);
            char* buffer = (char*)malloc(buf.dat_len + 1);
            std::streamsize count = file->read(buffer, buf.dat_len);
            if (count < buf.dat_len)
                buffer[count] = '\0';
            else
                buffer[buf.dat_len] = '\0';
            std::string result = buffer;
            free(buffer);

            // Check to make sure it is valid.
            if (count != buf.dat_len)
                throw Exception::InternalInconsistency();
            
            return result;
        }
        else
            throw Exception::NotSupported();
    }

    void FS::mknod(std::string path, mode_t mode, dev_t devid)
    {
        auto configuration = [&](LowLevel::INode& buf)
        {
            buf.dev = MINOR(devid);
            buf.rdev = MAJOR(devid);
        };
        this->performCreation(LowLevel::INodeType::INT_DEVICE, path, mode, configuration);
    }

    void FS::mkdir(std::string path, mode_t mode)
    {
        auto configuration = [&](LowLevel::INode& buf)
        {
        };
        this->performCreation(LowLevel::INodeType::INT_DIRECTORY, path, mode, configuration);
    }

    void FS::unlink(std::string path)
    {
        LowLevel::INode child, parent;
        if (!this->retrievePathToINode(path, child))
            throw Exception::FileNotFound();
        if (!this->retrieveParentPathToINode(path, parent))
            throw Exception::FileNotFound();
        
        // Ensure the inode is the correct type.
        if (child.type == LowLevel::INodeType::INT_DIRECTORY)
            throw Exception::IsADirectory();
        else if (child.type != LowLevel::INodeType::INT_FILEINFO &&
                child.type != LowLevel::INodeType::INT_SYMLINK &&
                child.type != LowLevel::INodeType::INT_DEVICE)
            throw Exception::InternalInconsistency();

        // Get the inode's position.
        uint32_t pos = this->filesystem->getINodePositionByID(child.inodeid);
        if (pos == 0)
            throw Exception::InternalInconsistency();

        // Get the real inode (if this is a hardlink).
        int result = 0;
        LowLevel::INode real = child.resolve(this->filesystem);

        // Remove the inode from the directory.
        LowLevel::FSResult::FSResult res = this->filesystem->removeChildFromDirectoryINode(parent.inodeid, real.inodeid);
        if (res == LowLevel::FSResult::E_FAILURE_NOT_A_DIRECTORY)
            throw Exception::NotADirectory();
        else if (res == LowLevel::FSResult::E_FAILURE_INVALID_FILENAME)
            throw Exception::FileNotFound();
        else if (res != LowLevel::FSResult::E_SUCCESS)
            throw Exception::InternalInconsistency();

        // If the real inode is a hardlink, we need to reset
        // the hardlink block
        if (real.inodeid != child.inodeid)
        {
            uint32_t rpos = this->filesystem->getINodePositionByID(real.inodeid);
            if (this->filesystem->resetBlock(rpos) != LowLevel::FSResult::E_SUCCESS)
                throw Exception::InternalInconsistency();
            if (this->filesystem->setINodePositionByID(real.inodeid, 0) != LowLevel::FSResult::E_SUCCESS)
                throw Exception::InternalInconsistency();
        }

        // Now reduce the nlink value by 1.
        child.nlink -= 1;
        child.ctime = this->getTime();
        if (child.nlink == 0)
        {
            // Erase all of the file segments first.
            this->filesystem->truncateFile(child.inodeid, 0);

            // Now reset the block and release the inode ID.
            if (this->filesystem->resetBlock(pos) != LowLevel::FSResult::E_SUCCESS)
                throw Exception::InternalInconsistency();
            if (this->filesystem->setINodePositionByID(child.inodeid, 0) != LowLevel::FSResult::E_SUCCESS)
                throw Exception::InternalInconsistency();
        }
        else
        {
            // Otherwise just save the new nlink value.
            this->saveINode(child);
        }
    }

    void FS::rmdir(std::string path)
    {
        LowLevel::INode child, parent;
        if (!this->retrievePathToINode(path, child))
            throw Exception::FileNotFound();
        if (!this->retrieveParentPathToINode(path, parent))
            throw Exception::FileNotFound();
        
        // Ensure the inode is the correct type.
        if (child.type != LowLevel::INodeType::INT_DIRECTORY)
            throw Exception::NotADirectory();

        // Ensure the directory is empty.
        if (child.children_count != 0)
            throw Exception::DirectoryNotEmpty();

        // Get the inode's position.
        uint32_t pos = this->filesystem->getINodePositionByID(child.inodeid);
        if (pos == 0)
            throw Exception::InternalInconsistency();

        // Remove the inode from the directory.
        LowLevel::FSResult::FSResult res = this->filesystem->removeChildFromDirectoryINode(parent.inodeid, child.inodeid);
        if (res == LowLevel::FSResult::E_FAILURE_NOT_A_DIRECTORY)
            throw Exception::NotADirectory();
        else if (res == LowLevel::FSResult::E_FAILURE_INVALID_FILENAME)
            throw Exception::FileNotFound();
        else if (res != LowLevel::FSResult::E_SUCCESS)
            throw Exception::InternalInconsistency();

        // Now reset the block and release the inode ID.
        if (this->filesystem->resetBlock(pos) != LowLevel::FSResult::E_SUCCESS)
            throw Exception::InternalInconsistency();
        if (this->filesystem->setINodePositionByID(child.inodeid, 0) != LowLevel::FSResult::E_SUCCESS)
            throw Exception::InternalInconsistency();
    }

    void FS::symlink(std::string linkPath, std::string targetPath)
    {
        auto configuration = [&](LowLevel::INode& buf)
        {
        };
        this->performCreation(LowLevel::INodeType::INT_SYMLINK, linkPath, 0755, configuration);

        // We just created the file, so if this fails then
        // it's an internal inconsistency.
        LowLevel::INode buf;
        if (!this->retrievePathToINode(linkPath, buf))
            throw Exception::FileNotFound();
       
        try
        {
            FSFile f = this->filesystem->getFile(buf.inodeid);
            f.open();
            f.write(targetPath.c_str(), targetPath.length());
            f.close();
            if (f.fail() || f.bad())
                throw Exception::InternalInconsistency();
        }
        catch (...)
        {
            // Delete the file from disk since we failed to
            // write to it (at least in some manner).  Then
            // rethrow the exception.
            this->unlink(linkPath);
            throw;
        }
    }

    void FS::rename(std::string srcPath, std::string destPath)
    {
        this->ensurePathRenamability(destPath, this->uid);
        this->ensurePathExists(srcPath);

        LowLevel::INode child, srcParent, destParent;
        if (!this->retrievePathToINode(srcPath, child))
            throw Exception::FileNotFound();
        if (!this->retrieveParentPathToINode(srcPath, srcParent))
            throw Exception::FileNotFound();
        if (!this->retrieveParentPathToINode(destPath, destParent))
            throw Exception::FileNotFound();

        // If the destination exists, then we are permitted
        // to rename (due to ensurePathRenamability), but we
        // must unlink or rmdir first.
        LowLevel::INode prev;
        if (this->retrievePathToINode(destPath, prev))
        {
            if (prev.type == LowLevel::INodeType::INT_DIRECTORY)
                this->rmdir(destPath);
            else
                this->unlink(destPath);
        }

        // Check if the directory owner needs to change.
        if (srcParent.inodeid != destParent.inodeid)
        {
            LowLevel::FSResult::FSResult res;

            // Add the new parent -> child relationship on disk.
            res = this->filesystem->addChildToDirectoryINode(destParent.inodeid, child.inodeid);
            if (res == LowLevel::FSResult::E_FAILURE_NOT_A_DIRECTORY)
                throw Exception::NotADirectory();
            else if (res == LowLevel::FSResult::E_FAILURE_MAXIMUM_CHILDREN_REACHED)
                throw Exception::DirectoryChildLimitReached();
            else if (res != LowLevel::FSResult::E_SUCCESS)
                throw Exception::InternalInconsistency();

            // Remove the old parent -> child relationship on disk.
            res = this->filesystem->removeChildFromDirectoryINode(srcParent.inodeid, child.inodeid);
            if (res == LowLevel::FSResult::E_FAILURE_NOT_A_DIRECTORY)
                throw Exception::NotADirectory();
            else if (res == LowLevel::FSResult::E_FAILURE_INVALID_FILENAME)
                throw Exception::FileNotFound();
            else if (res != LowLevel::FSResult::E_SUCCESS)
                throw Exception::InternalInconsistency();
        }
        
        // Change the filename.
        child.setFilename(LowLevel::Util::extractBasenameFromPath(destPath).c_str());
        this->touchINode(child, "c");
        this->saveINode(child);
    }

    void FS::link(std::string linkPath, std::string targetPath)
    {
        this->ensurePathIsAvailable(linkPath);
        this->ensurePathExists(targetPath);

        LowLevel::INode child;
        if (!this->retrievePathToINode(targetPath, child))
            throw Exception::FileNotFound();
        
        // Ensure the target is a plain old file.
        if (child.type == LowLevel::INodeType::INT_DIRECTORY)
            throw Exception::IsADirectory();
        else if (child.type != LowLevel::INodeType::INT_FILEINFO &&
                child.type != LowLevel::INodeType::INT_DEVICE)
            throw Exception::NotSupported();

        // Retrieve inode of link's parent.
        LowLevel::INode parent;
        if (!this->retrieveParentPathToINode(linkPath, parent))
            throw Exception::FileNotFound();

        // Create the new hardlink.
        auto configuration = [&](LowLevel::INode& buf)
        {
            buf.realid = child.inodeid;
        };
        this->performCreation(LowLevel::INodeType::INT_HARDLINK, linkPath, 0000, configuration);
        
        // Increase the nlink and save.
        child.nlink += 1;
        this->touchINode(child, "c");
        this->saveINode(child);
    }

    void FS::chmod(std::string path, mode_t mode)
    {
        LowLevel::INode child;
        if (!this->retrievePathToINode(path, child))
            throw Exception::FileNotFound();
        child.mask = this->extractMaskFromMode(mode);
        this->touchINode(child, "ca");
        this->saveINode(child);
    }

    void FS::chown(std::string path, uid_t uid, gid_t gid)
    {
        LowLevel::INode child;
        if (!this->retrievePathToINode(path, child))
            throw Exception::FileNotFound();
        if (uid != -1)
            child.uid = uid;
        if (gid != -1)
            child.gid = gid;
        this->touchINode(child, "ca");
        this->saveINode(child);
    }

    void FS::truncate(std::string path, off_t size)
    {
        if (size > MSIZE_FILE)
            throw Exception::FileTooBig();
        this->ensurePathExists(path);

        LowLevel::INode buf;
        if (!this->retrievePathToINode(path, buf))
            throw Exception::FileNotFound();
        this->touchINode(buf, "cma");
        this->saveINode(buf);

        // Truncate the file to the desired size.
        FSFile file = this->filesystem->getFile(buf.inodeid);
        file.open();
        file.truncate(size);
        file.close();
        if (file.fail() || file.bad())
            throw Exception::InternalInconsistency();
    }

    FSFile FS::open(std::string path)
    {
        this->ensurePathExists(path);

        LowLevel::INode buf;
        if (!this->retrievePathToINode(path, buf))
            throw Exception::FileNotFound();

        // Open the file and return it.
        FSFile file = this->filesystem->getFile(buf.inodeid);
        file.open();
        return file;
    }

    std::vector<std::string> FS::readdir(std::string path)
    {
        this->ensurePathExists(path);

        LowLevel::INode buf;
        if (!this->retrievePathToINode(path, buf))
            throw Exception::FileNotFound();
        if (buf.type != LowLevel::INodeType::INT_DIRECTORY)
            throw Exception::NotADirectory();

        // Copy the filename results from getChildrenOfDirectory.
        std::vector<std::string> result;
        std::vector<LowLevel::INode> children =
            this->filesystem->getChildrenOfDirectory(buf.inodeid);
        for (int i = 0; i < children.size(); i++)
            result.insert(result.end(), children[i].filename);
        return result;
    }

    void FS::create(std::string path, mode_t mode)
    {
        auto configuration = [&](LowLevel::INode& buf)
        {
        };
        this->performCreation(LowLevel::INodeType::INT_FILEINFO, path, mode, configuration);
    }

    void FS::utimens(std::string path, time_t access, time_t modification)
    {
        this->ensurePathExists(path);

        LowLevel::INode buf;
        if (!this->retrievePathToINode(path, buf))
            throw Exception::FileNotFound();
        buf.atime = access;
        buf.mtime = modification;
        this->saveINode(buf);
    }

    void FS::setuid(uid_t uid)
    {
        this->uid = uid;
    }

    void FS::setgid(gid_t gid)
    {
        this->gid = gid;
    }

    void FS::touch(std::string path, std::string modes)
    {
        LowLevel::INode child;
        if (!this->retrievePathToINode(path, child))
            throw Exception::FileNotFound();
        this->touchINode(child, modes);
        this->saveINode(child);
    }

    /****
     *
     * PRIVATE METHODS!
     *
     ****/

    void FS::ensurePathIsValid(std::string path) const
    {
        std::vector<std::string> components = LowLevel::Util::splitPathBySeperators(path);
        LowLevel::FSResult::FSResult res = LowLevel::Util::verifyPath(path, components);
        if (res == LowLevel::FSResult::E_FAILURE_INVALID_PATH)
            throw Exception::PathNotValid();
        else if (res == LowLevel::FSResult::E_FAILURE_INVALID_FILENAME)
            throw Exception::FilenameTooLong();
        else if (res != LowLevel::FSResult::E_SUCCESS)
            throw Exception::InternalInconsistency();
    }

    void FS::ensurePathExists(std::string path) const
    {
        this->ensurePathIsValid(path);
        std::vector<std::string> components = LowLevel::Util::splitPathBySeperators(path);
        LowLevel::INode buf = this->filesystem->getINodeByID(0);
        for (unsigned int i = 0; i < components.size(); i++)
        {
            buf = this->filesystem->getChildOfDirectory(buf.inodeid, components[i]);
            if (buf.type == LowLevel::INodeType::INT_INVALID)
                throw Exception::FileNotFound();
        }
    }

    void FS::ensurePathIsAvailable(std::string path) const
    {
        this->ensurePathIsValid(path);
        std::vector<std::string> components = LowLevel::Util::splitPathBySeperators(path);
        LowLevel::INode buf = this->filesystem->getINodeByID(0);
        for (unsigned int i = 0; i < components.size() - 1; i++)
        {
            buf = this->filesystem->getChildOfDirectory(buf.inodeid, components[i]);
            if (buf.type == LowLevel::INodeType::INT_INVALID)
                throw Exception::FileNotFound();
        }
        if (components.size() != 0)
            buf = this->filesystem->getChildOfDirectory(buf.inodeid,
                    components[components.size() - 1].c_str());
        if (buf.type == LowLevel::INodeType::INT_INVALID)
            return;
        throw Exception::FileExists();
    }

    void FS::ensurePathRenamability(std::string path, uid_t uid) const
    {
        // If the path doesn't exist, we don't need to check the special
        // sticky conditions.
        try
        {
            this->ensurePathIsAvailable(path);
            return;
        }
        catch (Exception::FileExists& e)
        {
            // The file exists, so we must continue checking
            // permissions.
        }

        // So the path does exist, and we need to check the modes on the
        // owning directory and the file.
        LowLevel::INode child, parent;
        if (!this->retrievePathToINode(path, child))
            throw Exception::FileNotFound();
        if (!this->retrieveParentPathToINode(path, parent))
            throw Exception::FileNotFound();
        if ((parent.mask & S_ISVTX) && !(child.uid == uid || parent.uid == uid))
            throw Exception::AccessDenied();

        // Otherwise, the user is permitted to do this.
    }

    bool FS::checkPermission(std::string path, char op, uid_t uid, gid_t gid) const
    {
        // FIXME: Implement this function.
        this->ensurePathIsValid(path);
        return true;
    }

    bool FS::retrievePathToINode(std::string path, LowLevel::INode& out, int limit) const
    {
        this->ensurePathIsValid(path);
        std::vector<std::string> components = LowLevel::Util::splitPathBySeperators(path);
        out = this->filesystem->getINodeByID(0);
        for (unsigned int i = 0; i < (limit <= 0 ? components.size() - (-limit) : limit); i++)
        {
            out = this->filesystem->getChildOfDirectory(out.inodeid, components[i]);
            if (out.type == LowLevel::INodeType::INT_INVALID)
                return false;
        }
        if (out.type == LowLevel::INodeType::INT_INVALID)
            return false;
        if (out.type == LowLevel::INodeType::INT_HARDLINK)
            out = out.resolve(this->filesystem);
        return true;
    }

    bool FS::retrieveParentPathToINode(std::string path, LowLevel::INode& out) const
    {
        return this->retrievePathToINode(path, out, -1);
    }

    void FS::saveINode(LowLevel::INode& buf)
    {
        if (buf.type == LowLevel::INodeType::INT_INVALID ||
                buf.type == LowLevel::INodeType::INT_UNSET)
            throw Exception::INodeSaveInvalid();
        if (this->filesystem->updateINode(buf) != LowLevel::FSResult::E_SUCCESS)
            throw Exception::INodeSaveFailed();
    }

    void FS::saveNewINode(uint32_t pos, LowLevel::INode& buf)
    {
        if (buf.type == LowLevel::INodeType::INT_INVALID ||
                buf.type == LowLevel::INodeType::INT_UNSET)
            throw Exception::INodeSaveInvalid();
        if (this->filesystem->writeINode(pos, buf) != LowLevel::FSResult::E_SUCCESS)
            throw Exception::INodeSaveFailed();
    }

    int FS::extractMaskFromMode(mode_t mode) const
    {
        if (mode & S_IFDIR)
            return mode & ~S_IFDIR;
        else if (mode & S_IFLNK)
            return mode & ~S_IFLNK;
        else if (mode & S_IFREG)
            return mode & ~S_IFREG;
        else
            return mode; // Keep other information.
    }

    LowLevel::INode FS::assignNewINode(LowLevel::INodeType::INodeType type, uint32_t& posOut)
    {
        if (type == LowLevel::INodeType::INT_INVALID ||
                type == LowLevel::INodeType::INT_UNSET)
            throw Exception::INodeSaveInvalid();
        
        // Determine appropriate type.
        LowLevel::INodeType::INodeType simple = LowLevel::INodeType::INT_FILEINFO;
        if (type == LowLevel::INodeType::INT_DIRECTORY)
            simple = type;

        // Get a free block.
        posOut = this->filesystem->getFirstFreeBlock(simple);
        if (posOut == 0)
            throw Exception::NoFreeSpace();

        // Get a free inode number.
        uint16_t id = this->filesystem->getFirstFreeINodeNumber();
        if (id == 0)
            throw Exception::INodeExhaustion();
        LowLevel::INode buf;
        buf.inodeid = id;
        buf.type = type;

        // Ensure that we don't have an invalid position and that the
        // inode ID is not already assigned.
        if (posOut < OFFSET_DATA)
            throw Exception::INodeSaveInvalid();
        if (type != LowLevel::INodeType::INT_SEGINFO &&
                type != LowLevel::INodeType::INT_FREELIST &&
                this->filesystem->getINodePositionByID(id) != 0)
            throw Exception::INodeSaveInvalid();

        // Reserve the inode ID with this INode's position.
        this->filesystem->reserveINodeID(id);

        // Return the new INode.  If any future operation fails before
        // this inode is written to disk, it is the callee's responsibility
        // to ensure the ID is freed.
        return buf;
    }

    time_t FS::getTime() const
    {
        return time(NULL);
    }

    void FS::touchINode(LowLevel::INode& node, std::string modes)
    {
        if (modes.find('a') != -1)
            node.atime = this->getTime();
        if (modes.find('m') != -1)
            node.mtime = this->getTime();
        if (modes.find('c') != -1)
            node.ctime = this->getTime();
    }

    LowLevel::INode FS::performCreation(LowLevel::INodeType::INodeType type,
            std::string path, mode_t mode,
            std::function<void(LowLevel::INode&)> configuration)
    {
        this->ensurePathIsAvailable(path);

        LowLevel::INode parent;
        if (!this->retrieveParentPathToINode(path, parent))
            throw Exception::FileNotFound();

        uint32_t pos;
        LowLevel::INode child = this->assignNewINode(type, pos);
        try
        {
            child.mask = this->extractMaskFromMode(mode);
            child.ctime = this->getTime();
            child.mtime = this->getTime();
            child.atime = this->getTime();
            child.uid = this->uid;
            child.gid = this->gid;
            configuration(child);
            child.setFilename(LowLevel::Util::extractBasenameFromPath(path).c_str());
            this->saveNewINode(pos, child);
        }
        catch (...)
        {
            // Ensure INode release and rethrow.
            this->filesystem->unreserveINodeID(child.inodeid);
            throw;
        }

        // Now add the parent-child relationship.
        try
        {
            LowLevel::FSResult::FSResult res = this->filesystem->addChildToDirectoryINode(parent.inodeid, child.inodeid);
            if (res == LowLevel::FSResult::E_FAILURE_NOT_A_DIRECTORY)
                throw Exception::NotADirectory();
            else if (res == LowLevel::FSResult::E_FAILURE_MAXIMUM_CHILDREN_REACHED)
                throw Exception::DirectoryChildLimitReached();
            else if (res != LowLevel::FSResult::E_SUCCESS)
                throw Exception::InternalInconsistency();
        }
        catch (...)
        {
            // Ensure INode is freed and rethrow.
            this->filesystem->resetBlock(pos);
            throw;
        }

        return child;
    }
}
