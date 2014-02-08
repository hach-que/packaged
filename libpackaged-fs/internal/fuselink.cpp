/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>
#include <libpackaged-fs/internal/fuselink.h>
#include <libpackaged-fs/logging.h>
#include <string>
#include <time.h>
#include <linux/kdev_t.h>

namespace AppLib
{
    namespace FUSE
    {
        FS * FuseLink::filesystem = NULL;
        void (*FuseLink::continuefunc) (void) = NULL;

        Mounter::Mounter(std::string image, std::string mount,
                bool foreground, bool allow_other, void (*continuefunc) (void))
        {
            this->mountResult = -EALREADY;

            // Define the fuse_operations structure.
            static fuse_operations ops;
            ops.getattr = &FuseLink::getattr;
            ops.readlink = &FuseLink::readlink;
            ops.mknod = &FuseLink::mknod;
            ops.mkdir = &FuseLink::mkdir;
            ops.unlink = &FuseLink::unlink;
            ops.rmdir = &FuseLink::rmdir;
            ops.symlink = &FuseLink::symlink;
            ops.rename = &FuseLink::rename;
            ops.link = &FuseLink::link;
            ops.chmod = &FuseLink::chmod;
            ops.chown = &FuseLink::chown;
            ops.truncate = &FuseLink::truncate;
            ops.open = &FuseLink::open;
            ops.read = &FuseLink::read;
            ops.write = &FuseLink::write;
            ops.statfs = NULL;
            ops.flush = NULL;
            ops.release = NULL;
            ops.fsync = NULL;
            ops.setxattr = NULL;
            ops.getxattr = NULL;
            ops.listxattr = NULL;
            ops.removexattr = NULL;
            ops.opendir = NULL;
            ops.readdir = &FuseLink::readdir;
            ops.releasedir = NULL;
            ops.fsyncdir = NULL;
            ops.init = &FuseLink::init;
            ops.destroy = &FuseLink::destroy;
            ops.access = NULL;
            ops.create = &FuseLink::create;
            ops.ftruncate = NULL;
            ops.fgetattr = NULL;
            ops.lock = NULL;
            ops.utimens = &FuseLink::utimens;
            ops.bmap = NULL;
            ops.ioctl = NULL;
            ops.poll = NULL;

            // Attempt to open the package and set
            // continuation function.
            FuseLink::filesystem = new FS(image);
            FuseLink::continuefunc = continuefunc;

            // Mounts the specified disk image at the
            // specified mount path using FUSE.
            struct fuse_args fargs = FUSE_ARGS_INIT(0, NULL);

            if (fuse_opt_add_arg(&fargs, "appfs") == -1)
            {
                Logging::showErrorW("Unable to set FUSE options.");
                fuse_opt_free_args(&fargs);
                this->mountResult = -5;
                return;
            }

            if (foreground)
            {
                if (fuse_opt_add_arg(&fargs, "-f") == -1
#ifdef DEBUG
                    || fuse_opt_add_arg(&fargs, "-d") == -1
#endif
                    )
                {
                    Logging::showErrorW("Unable to set FUSE options.");
                    fuse_opt_free_args(&fargs);
                    this->mountResult = -5;
                    return;
                }
            }

            const char *normal_opts = "default_permissions,use_ino,attr_timeout=0,entry_timeout=0";
            const char *allow_opts = "allow_other,default_permissions,use_ino,attr_timeout=0,entry_timeout=0";
            const char *opts = NULL;
            if (allow_other)
            {
                Logging::showInfoW("Allowing other users access to filesystem.");
                opts = allow_opts;
            }
            else
                opts = normal_opts;

            if (fuse_opt_add_arg(&fargs, "-s") == -1 || fuse_opt_add_arg(&fargs, "-o") || fuse_opt_add_arg(&fargs, opts) == -1 || fuse_opt_add_arg(&fargs, mount.c_str()) == -1)
            {
                Logging::showErrorW("Unable to set FUSE options.");
                fuse_opt_free_args(&fargs);
                this->mountResult = -5;
                return;
            }

            FUSEData appfs_status;
            appfs_status.filesystem = FuseLink::filesystem;
            appfs_status.readonly = false;
            appfs_status.mount = mount;
            appfs_status.image = image;

            this->mountResult = fuse_main(fargs.argc, fargs.argv, &ops, &appfs_status);
        }

        int Mounter::getResult()
        {
            return this->mountResult;
        }
        
        void API::load(std::string image)
        {
            // Unload any existing package.
            if (FuseLink::filesystem != NULL)
                API::unload();

            // Attempt to open the package and set
            // continuation function.
            FuseLink::filesystem = new FS(image);
        }
        
        void API::unload()
        {
            if (FuseLink::filesystem != NULL)
            {
                delete FuseLink::filesystem;
                FuseLink::filesystem = NULL;
            }
        }

        int FuseLink::getattr(const char *path, struct stat *stbuf)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Create a new stat object in the stbuf position.
            memset(stbuf, 0, sizeof(struct stat));

            // Attempt to get attributes.
            try
            {
                FuseLink::filesystem->getattr(path, *stbuf);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "getattr");
            }
        }

        int FuseLink::readlink(const char *path, char *out, size_t size)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to read link information.
            try
            {
                std::string result = FuseLink::filesystem->readlink(path);
                for (size_t i = 0; i < size; i++)
                    out[i] = '\0';
                for (size_t i = 0; i < result.length() && i < size; i++)
                    out[i] = result[i];
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "readlink");
            }
        }

        int FuseLink::mknod(const char *path, mode_t mode, dev_t devid)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to create device node.
            try
            {
                FuseLink::filesystem->mknod(path, mode, devid);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "mknod");
            }
        }

        int FuseLink::mkdir(const char *path, mode_t mode)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to create directory.
            try
            {
                FuseLink::filesystem->mkdir(path, mode);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "mkdir");
            }
        }

        int FuseLink::unlink(const char *path)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to unlink file.
            try
            {
                FuseLink::filesystem->unlink(path);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "unlink");
            }
        }

        int FuseLink::rmdir(const char *path)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to remove directory.
            try
            {
                FuseLink::filesystem->rmdir(path);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "rmdir");
            }
        }

        int FuseLink::symlink(const char *target, const char *path)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to create symbolic link.
            try
            {
                FuseLink::filesystem->symlink(path, target);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "symlink");
            }
        }

        int FuseLink::rename(const char *src, const char *dest)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to rename file or directory.
            try
            {
                FuseLink::filesystem->rename(src, dest);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "rename");
            }
        }

        int FuseLink::link(const char *target, const char *path)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to create hard link.
            try
            {
                FuseLink::filesystem->link(path, target);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "link");
            }
        }

        int FuseLink::chmod(const char *path, mode_t mode)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to change permissions mask.
            try
            {
                FuseLink::filesystem->chmod(path, mode);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "chmod");
            }
        }

        int FuseLink::chown(const char *path, uid_t user, gid_t group)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to change ownership.
            try
            {
                FuseLink::filesystem->chown(path, user, group);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "chown");
            }
        }

        int FuseLink::truncate(const char *path, off_t size)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to truncate file.
            try
            {
                FuseLink::filesystem->truncate(path, size);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "truncate");
            }
        }

        int FuseLink::open(const char *path, struct fuse_file_info *options)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Open the file to see whether it exists.
            try
            {
                FSFile file = FuseLink::filesystem->open(path);
                file.close();
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "open");
            }
        }

        int FuseLink::read(const char *path, char *out, size_t length,
                off_t offset, struct fuse_file_info *options)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Read data from the file.
            try
            {
                if (offset > MSIZE_FILE || ((uint64_t) offset + (uint64_t) length) > MSIZE_FILE)
                    return -EFBIG;
                FuseLink::filesystem->touch(path, "a");
                FSFile file = FuseLink::filesystem->open(path);
                file.seekg(offset);
                uint32_t read = file.read(out, length);
                file.close();
                if (file.fail() || file.bad())
                    return -EIO;
                return read;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "read");
            }
        }

        int FuseLink::write(const char *path, const char *in, size_t length,
                off_t offset, struct fuse_file_info *options)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Write data to the file.
            try
            {
                if (offset > MSIZE_FILE || ((uint64_t) offset + (uint64_t) length) > MSIZE_FILE)
                    return -EFBIG;
                FuseLink::filesystem->touch(path, "cma");
                FSFile file = FuseLink::filesystem->open(path);
                file.seekp(offset);
                file.write(in, length);
                file.close();
                if (file.fail() || file.bad())
                    return -EIO;
                return length;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "write");
            }
        }

        int FuseLink::readdir(const char *path, void *dbuf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // List all children in a directory.
            try
            {
                // Retrieve all children.
                std::vector<std::string> result =
                    FuseLink::filesystem->readdir(path);
                
                // Use the filler() function to report back entries.
                filler(dbuf, ".", NULL, 0);
                filler(dbuf, "..", NULL, 0);
                for (int i = 0; i < result.size(); i++)
                    filler(dbuf, result[i].c_str(), NULL, 0);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "readdir");
            }
        }

        void *FuseLink::init(struct fuse_conn_info *conn)
        {
            if (FuseLink::continuefunc != NULL)
            {
                FuseLink::continuefunc();
            }

            return NULL;
        }

        void FuseLink::destroy(void *)
        {
            return;
        }

        int FuseLink::create(const char *path, mode_t mode, struct fuse_file_info *options)
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Attempt to create normal file.
            try
            {
                FuseLink::filesystem->create(path, mode);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "create");
            }
        }

        int FuseLink::utimens(const char *path, const struct timespec tv[2])
        {
            FuseLink::filesystem->setuid(fuse_get_context()->uid);
            FuseLink::filesystem->setgid(fuse_get_context()->gid);

            // Set the access and modification times.
            try
            {
                FuseLink::filesystem->utimens(path, tv[0].tv_sec, tv[1].tv_sec);
                return 0;
            }
            catch (std::exception& e)
            {
                return FuseLink::handleException(e, "utimens");
            }
        }

        int FuseLink::handleException(std::exception& e, std::string function)
        {
            if (typeid(e) == typeid(Exception::PathNotValid&))
                return -EIO;
            if (typeid(e) == typeid(Exception::FileNotFound&))
                return -ENOENT;
            if (typeid(e) == typeid(Exception::NoFreeSpace&))
                return -ENOSPC;
            if (typeid(e) == typeid(Exception::AccessDenied&))
                return -EACCES;
            if (typeid(e) == typeid(Exception::PathNotValid&))
                return -EIO;
            if (typeid(e) == typeid(Exception::FileExists&))
                return -EEXIST;
            if (typeid(e) == typeid(Exception::NotADirectory&))
                return -ENOTDIR;
            if (typeid(e) == typeid(Exception::IsADirectory&))
                return -EISDIR;
            if (typeid(e) == typeid(Exception::DirectoryNotEmpty&))
                return -ENOTEMPTY;
            if (typeid(e) == typeid(Exception::FileTooBig&))
                return -EFBIG;
            if (typeid(e) == typeid(Exception::NotSupported&))
                return -ENOTSUP;
            if (typeid(e) == typeid(Exception::FilenameTooLong&))
                return -ENAMETOOLONG;
            if (typeid(e) == typeid(Exception::INodeSaveInvalid&) ||
                    typeid(e) == typeid(Exception::INodeSaveFailed&) ||
                    typeid(e) == typeid(Exception::INodeExhaustion&) ||
                    typeid(e) == typeid(Exception::InternalInconsistency&) ||
                    typeid(e) == typeid(Exception::DirectoryChildLimitReached&))
                return -EIO;

            // Exception was unknown!
            Logging::showErrorW("Unhandled exception occurred in FuseLink::%s.", function.c_str());
            Logging::showErrorO("-> '%s'", e.what());
            return -EIO;
        }
    }
}
