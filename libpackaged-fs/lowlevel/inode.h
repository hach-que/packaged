/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_LOWLEVEL_INODE
#define CLASS_LOWLEVEL_INODE

#include <libpackaged-fs/config.h>

namespace AppLib
{
    namespace LowLevel
    {
        class INode;
    }
}

#include <string>
#include <libpackaged-fs/lowlevel/inodetype.h>
#include <libpackaged-fs/lowlevel/fs.h>

namespace AppLib
{
    namespace LowLevel
    {
        class INode
        {
        public:
            uint16_t inodeid;
            char filename[256];
            INodeType::INodeType type;
            uint16_t uid;
            uint16_t gid;
            uint16_t mask;
            uint64_t atime;
            uint64_t mtime;
            uint64_t ctime;
            uint16_t parent;
            uint16_t children[DIRECTORY_CHILDREN_MAX];
            uint16_t children_count;
            uint16_t dev;
            uint16_t rdev;
            uint16_t nlink;
            uint16_t blocks;
            uint32_t dat_len;
            uint32_t info_next;
            uint32_t flst_next;
            uint16_t realid;
            char realfilename[256]; //!< In-memory only (never written to disk).

            // FSInfo only
            char fs_name[10];
            uint16_t ver_major;
            uint16_t ver_minor;
            uint16_t ver_revision;
            char app_name[256];
            char app_ver[32];
            char app_desc[1024];
            char app_author[256];
            uint32_t pos_root;
            uint32_t pos_freelist;

            INode(uint16_t id, const char *filename, INodeType::INodeType type, uint16_t uid, uint16_t gid, uint16_t mask, uint64_t atime, uint64_t mtime, uint64_t ctime);
            INode(uint16_t id = 0, const char *filename = "", INodeType::INodeType type = INodeType::INT_UNSET);
            ~INode();
            std::string getBinaryRepresentation();
            void setFilename(const char *name, const char *real = "");
            void setAppName(const char *name);
            void setAppVersion(const char *name);
            void setAppDesc(const char *name);
            void setAppAuthor(const char *name);

            //! Ensures that the node data is valid.
            bool verify();

            //! Resolves a hardlink to the real file.
            INode resolve(FS* filesystem);

        private:
            static void copyFilename(char from[256], char to[256]);
        };
    }
}

#endif
