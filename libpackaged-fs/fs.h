/* vim: set ts=4 sw=4 tw=0 :*/

#ifndef CLASS_FS
#define CLASS_FS

#include <libpackaged-fs/config.h>

#include <string>
#include <cstdio>
#include <functional>
#include <libpackaged-fs/fsfile.h>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <libpackaged-fs/lowlevel/fs.h>
#include <libpackaged-fs/exception/package.h>
#include <libpackaged-fs/exception/fs.h>
#include <libpackaged-fs/exception/util.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace AppLib
{
    class FS
    {
    private:
        AppLib::LowLevel::BlockStream * stream;
        AppLib::LowLevel::FS * filesystem;
        uid_t uid;
        gid_t gid;

    public:
        //! Opens an existing package.
        /*!
         * Opens a existing package at the specified path.  The
         * optional uid and gid parameters effectively perform
         * setuid and setgid for you.
         *
         * @param path The path to open the package at.
         * @param uid The context user ID to set for package operations.
         * @param gid The context group ID to set for package operations.
         *
         * @throw Exception::PackageNotFound
         * @throw Exception::PackageNotValid
         */
        FS(std::string packagePath, uid_t uid = 0, gid_t gid = 0);
        //! Retrieves attributes on a file or directory.
        /*!
         * Retrieves attributes on a file, directory, device or
         * symlink.  The equivalent of the getattr() operation used
         * for standard filesystems.
         *
         * @param path The path to get attributes of.
         * @param stbufOut The structure to store the result in.
         *
         * @throw Exception::PathNotValid
         * @throw Exception::FileNotFound
         * @throw Exception::InternalInconsistency
         */
        void getattr(std::string path, struct stat& stbufOut) const;
        //! Returns the target of a symbolic link.
        /*!
         * Returns the target of a symbolic link. The equivalent
         * of the readlink() operation used for standard
         * filesystems.
         *
         * @param path The path of the symlink to read.
         *
         * @throw Exception::PathNotValid
         * @throw Exception::FileNotFound
         * @throw Exception::NotSupported
         * @throw Exception::InternalInconsistency
         */
        std::string readlink(std::string path) const;
        //! Creates a device node in the package.
        /*!
         * Creates a new device node in the package.  The equivalent
         * of the mknod() operation used for standard filesystems.
         *
         * @note Unless the kernel permits it (such as the user
         *       running the package is root), device nodes will not be
         *       shown when mounted under FUSE.
         *
         * @param path The path to create the device node at.
         * @param mode The permissions mode to create the node with.
         * @param devid The device minor and major numbers.
         */
        void mknod(std::string path, mode_t mode, dev_t devid);
        //! Creates a directory in the package.
        /*!
         * Creates a new directory in the package.  The equivalent
         * of the mkdir() operation used for standard filesystems.
         *
         * @note Directories will not be created recursively by this
         *       function.  The parent path must already exist.
         *
         * @param path The directory path to create.
         * @param mode The permissions mode to create the directory with.
         */
        void mkdir(std::string path, mode_t mode);
        //! Unlinks a file from the package.
        /*!
         * Unlinks a file, symlink or device node (not a directory)
         * from the package.  The equivalent of the unlink() operation
         * used for standard filesystems.
         *
         * @note This only unlinks a file, symlink or device.  It does
         *       not actually free the disk space if there are other
         *       hard links to the file data.  Only when nlink reaches
         *       0 due to an unlink will the associated file data be
         *       freed.
         *
         * @param path The path to unlink.
         *
         * @throw Exception::PathNotValid
         * @throw Exception::FileNotFound
         * @throw Exception::IsADirectory
         * @throw Exception::InternalInconsistency
         * @throw Exception::NotADirectory
         */
        void unlink(std::string path);
        //! Removes a directory from the package.
        /*!
         * Removes a directory from the package.  The equivalent
         * of the rmdir() operation used for standard filesystems.
         *
         * @note The directory must be empty when performing this
         *       operation.
         *
         * @param path The directory path to remove.
         *
         * @throw Exception::PathNotValid
         * @throw Exception::FileNotFound
         * @throw Exception::NotADirectory
         * @throw Exception::DirectoryNotEmpty
         * @throw Exception::InternalInconsistency
         */
        void rmdir(std::string path);
        //! Creates a symbolic link in the package.
        /*!
         * Creates a new symbolic link at the specified link
         * path.  The link will point to target path.  The
         * equivalent of the symlink() operation used for
         * standard filesystems.
         *
         * @param linkPath The link to be created.
         * @param targetPath The target of the link.
         *
         * @throw Exception::FileNotFound
         * @throw Exception::InternalInconsistency
         */
        void symlink(std::string linkPath, std::string targetPath);
        //! Renames a file in the package.
        /*!
         * Renames a file in the package.  The equivalent of
         * the rename() operation used for standard filesystems.
         *
         * @note This operation is also used to move files.
         *
         * @param srcPath The full path to the current file.
         * @param destPath The full path to the destination file.
         *
         * @throw Exception::AccessDenied
         * @throw Exception::FileNotFound
         * @throw Exception::NotADirectory
         * @throw Exception::DirectoryChildLimitReached
         * @throw Exception::InternalInconsistency
         */
        void rename(std::string srcPath, std::string destPath);
        //! Creates a hard link in the package.
        /*!
         * Creates a hard link to a file in the package. The
         * equivalent of the link() operation used for
         * standard filesystems.
         *
         * @param linkPath The link to be created.
         * @param targetPath The target of the link.
         *
         * @throw Exception::FileExists
         * @throw Exception::FileNotFound
         * @throw Exception::IsADirectory
         * @throw Exception::NotSupported
         */
        void link(std::string linkPath, std::string targetPath);
        //! Changes the permissions on a file in the package.
        /*!
         * Changes the permission mask on a file, directory,
         * device or symlink in the package.  The equivalent of
         * the chmod() operation used for standard filesystems.
         *
         * @param path The path to the file to change.
         * @param mask The permissions mask to set.
         *
         * @throw Exception::FileNotFound
         */
        void chmod(std::string path, mode_t mask);
        //! Changes the ownership of a file in the package.
        /*!
         * Changes the ownership of a file, directory, device
         * or symlink in the package.  The equivalent of
         * the chown() operation used for standard filesystems.
         *
         * @param path The path to the file to change.
         * @param uid The user ID to set, or -1 to leave as-is.
         * @param gid The group ID to set, or -1 to leave as-is.
         *
         * @throw Exception::FileNotFound
         */
        void chown(std::string path, uid_t uid = -1, gid_t gid = -1);
        //! Truncates a file in the package to a specified size.
        /*!
         * Truncates a file in the package to a specified size.
         * The equivalent of the truncate() operation used for
         * standard filesystems.
         *
         * @note It is faster to truncate a file to the correct
         *       size before writing data if you know the final
         *       size of the file after writing.
         *
         * @param path The path to the file to truncate.
         * @param size The new size of the file.
         *
         * @throw Exception::FileTooBig
         * @throw Exception::FileNotFound
         * @throw Exception::InternalInconsistency
         */
        void truncate(std::string path, off_t size);
        //! Opens the file in the package and returns an FSFile.
        /*!
         * Opens a file in the package and returns an FSFile which
         * can be used to read and write data to the file.
         *
         * @note This function will call open() on FSFile
         *       automatically.  You should not call open()
         *       on the returned FSFile.
         *
         * @param path The path to the file to open.
         *
         * @throw Exception::FileNotFound
         */
        FSFile open(std::string path);
        //! Lists the entries in a directory.
        /*!
         * Lists all of the entries in a directory excluding
         * '.' and '..' entries.
         *
         * @param path The path to the directory.
         *
         * @throw Exception::FileNotFound
         * @throw Exception::NotADirectory
         */
        std::vector<std::string> readdir(std::string path);
        //! Creates an empty file in the package.
        /*!
         * Creates a new normal, empty file.  The equivalent
         * of the create() operation used for standard filesystems.
         *
         * @param path The path to the file create.
         * @param mode The permissions mode to create the file with.
         */
        void create(std::string path, mode_t mode);
        //! Sets the access and modification times on a file.
        /*!
         * Sets the access and modification times of a file,
         * device, directory or symbolic link.  The equivalent
         * of the utimens() operation used for standard filesystems.
         *
         * @note The AppFS filesystem does not support nanosecond
         *       resolution on access or modification times.  Thus
         *       the parameters here indicate seconds.
         *
         * @param path The path to set times on.
         * @param access The access time to set (in seconds).
         * @param modification The modification time to set (in seconds).
         *
         * @throw Exception::FileNotFound
         */
        void utimens(std::string path, time_t access, time_t modification);

        /*!
         * Sets the current context UID for package operations.
         */
        void setuid(uid_t uid);
        /*!
         * Sets the current context GID for package operations.
         */
        void setgid(gid_t gid);

        /*!
         * Touches the specified file, updating each of the
         * specified times to the current time on the local
         * machine.
         *
         * @note This function saves the new times to disk.
         *
         * @param path The path to touch.
         * @param modes A string containing one or more of 'a', 'm' or 'c'.
         *
         * @throw Exception::FileNotFound
         */
        void touch(std::string path, std::string modes);

    private:
        /*!
         * Ensures the specified path is valid.
         *
         * @throw Exception::PathNotValid
         * @throw Exception::FilenameTooLong
         */
        void ensurePathIsValid(std::string path) const;
        /*!
         * Ensures the specified path exists.
         *
         * @throw Exception::FileNotFound
         * @throw Exception::PathNotValid
         * @throw Exception::FilenameTooLong
         */
        void ensurePathExists(std::string path) const;
        /*!
         * Ensures the specified path is available, that is
         * all required parent directories exist, but the
         * last component of the path does not.
         *
         * @throw Exception::FileNotFound
         * @throw Exception::FileExists
         * @throw Exception::PathNotValid
         * @throw Exception::FilenameTooLong
         */
        void ensurePathIsAvailable(std::string path) const;
        /*!
         * Ensures the specified path can be renamed by the
         * specified user.
         *
         * @throw Exception::FileNotFound
         * @throw Exception::FileExists
         * @throw Exception::PathNotValid
         * @throw Exception::FilenameTooLong
         * @throw Exception::AccessDenied
         */
        void ensurePathRenamability(std::string path, uid_t uid) const;
        /*!
         * Checks if the specified path can have the specified
         * operation performed on it by the specified user and
         * group.
         */
        bool checkPermission(std::string path, char op, uid_t uid, gid_t gid) const;
        /*!
         * Retrieves the inode represented by the path, storing
         * the result in out.  The optional limit parameter
         * indicates the number of path components to evaluate,
         * with negative values counting back from the end.
         */
        bool retrievePathToINode(std::string path, LowLevel::INode& out, int limit = 0) const;
        /*!
         * Retrieves the parent inode to the inode represented
         * by the path, storing the result in out.
         */
        bool retrieveParentPathToINode(std::string path, LowLevel::INode& out) const;
        /*!
         * Saves an existing inode to disk.
         * 
         * @throw Exception::INodeSaveInvalid
         * @throw Exception::INodeSaveFailed
         */
        void saveINode(LowLevel::INode& buf);
        /*!
         * Saves the new inode to disk.
         *
         * @throw Exception::INodeSaveInvalid
         * @throw Exception::INodeSaveFailed
         */
        void saveNewINode(uint32_t pos, LowLevel::INode& buf);
        /*!
         * Extracts the permissions mask from the full mode
         * information.
         */
        int extractMaskFromMode(mode_t mode) const;
        /*!
         * Assigns a new inode the specified type and stores the
         * position of the new inode into posOut.
         *
         * @throw Exception::INodeSaveInvalid
         * @throw Exception::INodeSaveFailed
         * @throw Exception::NoFreeSpace
         * @throw Exception::INodeExhaustion
         */
        LowLevel::INode assignNewINode(LowLevel::INodeType::INodeType type, uint32_t& posOut);
        /*!
         * Retrieves the current time on the local machine.
         */
        time_t getTime() const;
        /*!
         * Touches the specified inode, updating each of the
         * specified times to the current time on the local
         * machine.  Does not update the inode on disk.
         *
         * @note It is the callee's responsibility to save
         *       the inode to disk via saveINode.
         *
         * @param node The inode to touch.
         * @param modes A string containing one or more of 'a', 'm' or 'c'.
         */
        void touchINode(LowLevel::INode& node, std::string modes);

        /*!
         * Creates a new inode with the specified type at the
         * specified path, with the specified mode and calls
         * the configuration lambda on the new inode before
         * saving it to disk.
         *
         * This is a much safer alternative to call than
         * FS::assignNewINode as it ensures that the inode is
         * either written to disk successfully and correctly,
         * or is not written at all.
         *
         * @throw Exception::PathNotValid
         * @throw Exception::FileExists
         * @throw Exception::FileNotFound
         * @throw Exception::NotADirectory
         * @throw Exception::DirectoryChildLimitReached
         * @throw Exception::InternalInconsistency
         */
        LowLevel::INode performCreation(LowLevel::INodeType::INodeType type,
                std::string path, mode_t mode,
                std::function<void(LowLevel::INode&)> configuration);
    };
}

#endif
