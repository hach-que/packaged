/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>

#include <libpackaged-fs/lowlevel/inodetype.h>
#include <libpackaged-fs/lowlevel/inode.h>

namespace AppLib
{
    namespace LowLevel
    {
        INode::INode(uint16_t id, const char *filename, INodeType::INodeType type, uint16_t uid, uint16_t gid, uint16_t mask, uint64_t atime, uint64_t mtime, uint64_t ctime)
        {
            this->inodeid = id;
            this->setFilename(filename, "");
            this->type = type;
            this->uid = uid;
            this->gid = gid;
            this->mask = mask;
            this->atime = atime;
            this->mtime = mtime;
            this->ctime = ctime;
            this->dat_len = 0;
            this->info_next = 0;
            this->flst_next = 0;
            this->realid = 0;
            this->parent = 0;
            this->children_count = 0;
            this->dev = 0;
            this->rdev = 0;
            this->nlink = 1;	// we only have one reference to this object
            this->blocks = 0;
            for (uint16_t i = 0; i < DIRECTORY_CHILDREN_MAX; i += 1)
                this->children[i] = 0;

            // FSInfo only
            for (uint16_t i = 0; i < 10; i += 1)
                this->fs_name[i] = FS_NAME[i];
            this->ver_major = 0;
            this->ver_minor = 0;
            this->ver_revision = 0;
            for (uint16_t i = 0; i < 256; i += 1)
                this->app_name[i] = '\0';
            for (uint16_t i = 0; i < 32; i += 1)
                this->app_ver[i] = '\0';
            for (uint16_t i = 0; i < 1024; i += 1)
                this->app_desc[i] = '\0';
            for (uint16_t i = 0; i < 256; i += 1)
                this->app_author[i] = '\0';
            this->pos_root = 0;
            this->pos_freelist = 0;
        }

        INode::INode(uint16_t id, const char *filename, INodeType::INodeType type)
        {
            this->inodeid = id;
            this->setFilename(filename, "");
            this->type = type;
            this->uid = 0;
            this->gid = 0;
            this->mask = 0;
            this->atime = 0;
            this->mtime = 0;
            this->ctime = 0;
            this->dat_len = 0;
            this->info_next = 0;
            this->flst_next = 0;
            this->realid = 0;
            this->parent = 0;
            this->children_count = 0;
            this->dev = 0;
            this->rdev = 0;
            this->nlink = 1;	// we only have one reference to this object
            this->blocks = 0;
            for (uint16_t i = 0; i < DIRECTORY_CHILDREN_MAX; i += 1)
                this->children[i] = 0;

            // FSInfo only
            for (uint16_t i = 0; i < 10; i += 1)
                this->fs_name[i] = FS_NAME[i];
            this->ver_major = 0;
            this->ver_minor = 0;
            this->ver_revision = 0;
            for (uint16_t i = 0; i < 256; i += 1)
                this->app_name[i] = '\0';
            for (uint16_t i = 0; i < 32; i += 1)
                this->app_ver[i] = '\0';
            for (uint16_t i = 0; i < 1024; i += 1)
                this->app_desc[i] = '\0';
            for (uint16_t i = 0; i < 256; i += 1)
                this->app_author[i] = '\0';
            this->pos_root = 0;
            this->pos_freelist = 0;
        }

        INode::~INode()
        {
        }

        std::string INode::getBinaryRepresentation()
        {
            std::stringstream binary_rep;
            Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->inodeid), 2);
            Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->type), 2);
            if (this->type == INodeType::INT_SEGINFO)
            {
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->info_next), 4);
                 return binary_rep.str();
            }
            else if (this->type == INodeType::INT_FREELIST)
            {
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->flst_next), 4);
                return binary_rep.str();
            }
            else if (this->type == INodeType::INT_FSINFO)
            {
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->fs_name), 10);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->ver_major), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->ver_minor), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->ver_revision), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->app_name), 256);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->app_ver), 32);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->app_desc), 1024);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->app_author), 256);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->pos_root), 4);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->pos_freelist), 4);
                return binary_rep.str();
            }
            if ((this->type == INodeType::INT_FILEINFO || this->type == INodeType::INT_DEVICE) && this->realid != 0)
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->realfilename), 256);
            else
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->filename), 256);

            if (this->type != INodeType::INT_HARDLINK)
            {
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->uid), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->gid), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->mask), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->atime), 8);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->mtime), 8);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->ctime), 8);
            }
            if (this->type == INodeType::INT_FILEINFO || this->type == INodeType::INT_SYMLINK || this->type == INodeType::INT_DEVICE)
            {
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->dev), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->rdev), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->nlink), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->blocks), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->dat_len), 4);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->info_next), 4);
            }
            else if (this->type == INodeType::INT_DIRECTORY)
            {
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->parent), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->children_count), 2);
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->children), DIRECTORY_CHILDREN_MAX * 2);
            }
            else if (this->type == INodeType::INT_HARDLINK)
                Endian::doW(&binary_rep, reinterpret_cast < char *>(&this->realid), 2);
            return binary_rep.str();
        }

        void INode::setFilename(const char *name, const char *real)
        {
            uint16_t size = (strlen(name) < 255) ? strlen(name) : 255;
            for (uint16_t i = 0; i < size; i += 1)
                this->filename[i] = name[i];
            for (uint16_t i = size; i < 256; i += 1)
                this->filename[i] = '\0';
            uint16_t rsize = (strlen(real) < 255) ? strlen(real) : 255;
            for (uint16_t i = 0; i < rsize; i += 1)
                this->realfilename[i] = real[i];
            for (uint16_t i = size; i < 256; i += 1)
                this->realfilename[i] = '\0';
        }

        void INode::setAppName(const char *name)
        {
            uint16_t size = (strlen(name) < 255) ? strlen(name) : 255;
            for (uint16_t i = 0; i < size; i += 1)
                this->app_name[i] = name[i];
            for (uint16_t i = size; i < 256; i += 1)
                this->app_name[i] = '\0';
        }

        void INode::setAppVersion(const char *name)
        {
            uint16_t size = (strlen(name) < 31) ? strlen(name) : 31;
            for (uint16_t i = 0; i < size; i += 1)
                this->app_ver[i] = name[i];
            for (uint16_t i = size; i < 32; i += 1)
                this->app_ver[i] = '\0';
        }

        void INode::setAppDesc(const char *name)
        {
            uint16_t size = (strlen(name) < 1023) ? strlen(name) : 1023;
            for (uint16_t i = 0; i < size; i += 1)
                this->app_desc[i] = name[i];
            for (uint16_t i = size; i < 1024; i += 1)
                this->app_desc[i] = '\0';
        }

        void INode::setAppAuthor(const char *name)
        {
            uint16_t size = (strlen(name) < 255) ? strlen(name) : 255;
            for (uint16_t i = 0; i < size; i += 1)
                this->app_author[i] = name[i];
            for (uint16_t i = size; i < 256; i += 1)
                this->app_author[i] = '\0';
        }

        bool INode::verify()
        {
            if (this->filename[0] == 0 && (this->type == INodeType::INT_DIRECTORY || this->type == INodeType::INT_FILEINFO) && this->inodeid != 0)
                return false;
            return true;
        }

        INode INode::resolve(FS* filesystem)
        {
            if (this->type == INodeType::INT_HARDLINK && this->realid != 0)
            {
                Logging::showDebugW("Resolving hardlink %u to %u.", this->inodeid, this->realid);
                INode node = filesystem->getINodeByID(this->realid);
                node.realid = this->inodeid;
                INode::copyFilename(node.filename, node.realfilename);
                INode::copyFilename(this->filename, node.filename);
                return node;
            }
            else if ((this->type == INodeType::INT_FILEINFO || this->type == INodeType::INT_DEVICE) && this->realid != 0)
            {
                Logging::showDebugW("Resolving fileinfo %u back to hardlink %u.", this->inodeid, this->realid);
                INode node = filesystem->getRealINodeByID(this->realid);
                node.realid = this->inodeid;
                INode::copyFilename(this->filename, node.realfilename);
                return node;
            }
            else
                return *this;
        }

        void INode::copyFilename(char from[256], char to[256])
        {
            for (int i = 0; i < 256; i += 1)
                to[i] = 0;
            for (int i = 0; i < 255; i += 1)
            {
                if (from[i] == 0)
                    break;
                to[i] = from[i];
            }
        }
    }
}

