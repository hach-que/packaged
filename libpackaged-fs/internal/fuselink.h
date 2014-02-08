/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_FUSELINK
#define CLASS_FUSELINK

#include <libpackaged-fs/config.h>

#include <exception>
#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <libpackaged-fs/fs.h>

namespace AppLib
{
    namespace FUSE
    {
        class FuseLink
        {
        public:
            static FS * filesystem;
            static void (*continuefunc) (void);
            static int getattr(const char *path, struct stat *stbuf);
            static int readlink(const char *path, char *out, size_t size);
            static int mknod(const char *path, mode_t mask, dev_t devid);
            static int mkdir(const char *path, mode_t mask);
            static int unlink(const char *path);
            static int rmdir(const char *path);
            static int symlink(const char *path, const char *source);
            static int rename(const char *src, const char *dest);
            static int link(const char *path, const char *source);
            static int chmod(const char *path, mode_t mode);
            static int chown(const char *path, uid_t user, gid_t group);
            static int truncate(const char *path, off_t size);
            static int open(const char *path, struct fuse_file_info *options);
            static int read(const char *path, char *out, size_t length,
                            off_t offset, struct fuse_file_info *options);
            static int write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
            static int readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
            static void *init(struct fuse_conn_info *conn);
            static void destroy(void *);
            static int create(const char *, mode_t, struct fuse_file_info *);
            static int utimens(const char *, const struct timespec tv[2]);
        private:
            static int handleException(std::exception& e, std::string function);
        };

        class Mounter
        {
        public:
            Mounter(std::string image, std::string mount,
                    bool foreground, bool allowOther, void (*continue_func) (void));
            int getResult();

        private:
            int mountResult;
        };

        class API
        {
        public:
            static void load(std::string image);
            static void unload();
        };
        
        struct FUSEData
        {
            std::string image;
            std::string mount;
            AppLib::FS * filesystem;
            bool readonly;
        };
    }
}
#endif
