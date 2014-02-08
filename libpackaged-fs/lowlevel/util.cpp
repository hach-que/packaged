/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>

#include <string>
#include <iostream>
#include <fstream>
#include <libpackaged-fs/lowlevel/util.h>
#include <libpackaged-fs/logging.h>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <libpackaged-fs/lowlevel/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>

namespace AppLib
{
    namespace LowLevel
    {
        void Util::seekp_ex(LowLevel::BlockStream * fd, std::streampos pos)
        {
            fd->clear();
            fd->seekp(pos);
            if (fd->fail())
            {
                fd->clear();
                // This means that pos is beyond the current filesize.
                // We need to expand the file by writing NUL characters
                // until fd->tellp is equal to pos.
                fd->seekp(0, std::ios::end);
                if (fd->fail())
                {
                    Logging::showErrorW("Unable to expand the provided file desciptor to the desired position.");
                }
                char zero = 0;
                while (fd->tellp() != pos)
                {
                    fd->write(&zero, 1);
                }
            }
        }

        bool Util::fileExists(std::string filename)
        {
            struct stat file_info;
            return (stat(filename.c_str(), &file_info) == 0);
        }
        
        void Util::sanitizeArguments(char ** argv, int argc, std::string & command, int start)
        {
            for (int i = start; i < argc; i += 1)
            {
                // There must be a better way of replacing characters to
                // ensure the arguments are passed correctly.
                std::string insane_argv = argv[i];
                std::string sane_argv = "";
                for (int a = 0; a < insane_argv.length(); a += 1)
                {
                    if (insane_argv[a] == '\\')
                        sane_argv.append("\\\\");
                    else if (insane_argv[a] == '"')
                        sane_argv.append("\\\"");
                    else
                        sane_argv += insane_argv[a];
                }
                command = command + " \"" + sane_argv + "\"";
            }
        }

        bool Util::extractBootstrap(std::string source, std::string dest)
        {
            FILE * fsrc = fopen(source.c_str(), "r");
            if (fsrc == NULL) return false;
            FILE * fdst = fopen(dest.c_str(), "w");
            if (fdst == NULL) return false;
            char * buffer = (char*)malloc(1024*1024);
            size_t readres = fread(buffer, 1, 1024*1024, fsrc);
            if (ferror(fsrc) != 0) return false;
#ifdef WIN32
            size_t writeres = fwrite(buffer, 1, 1024*1024, fdst);
#else
            ssize_t writeres = fwrite(buffer, 1, 1024*1024, fdst);
#endif
            if (ferror(fdst) != 0) return false;
            if (fclose(fsrc) != 0) return false;
            if (fclose(fdst) != 0) return false;
            return true;
        }

        char* Util::getProcessFilename()
        {
            // This is non-portable code.  For each UNIX kernel this is
            // compiled on, a section needs to be added to handle fetching
            // the current process filename.  There are a list of ways for
            // each kernel available at:
            //	* http://stackoverflow.com/questions/933850
#ifdef WIN32
            return NULL;
#else
            char * buf = (char*)malloc(PATH_MAX+1);
            for (int i = 0; i < PATH_MAX + 1; i+=1)
                buf[i] = 0;
            int ret = readlink("/proc/self/exe", buf, PATH_MAX);
            if (ret == -1)
                return NULL;
            else
                return buf;
#endif
        }

        bool Util::createPackage(std::string path, const char* appname, const char* appver,
                            const char* appdesc, const char* appauthor)
        {
            // Open the new package.
            std::fstream * nfd = new std::fstream(path.c_str(),
                std::ios::out | std::ios::trunc | std::ios::in | std::ios::binary);
            if (!nfd->is_open())
            {
                AppLib::Logging::showErrorW("Unable to open new package path for writing.");
                return false;
            }

            // Write out 1MB for the bootstrap area.
#if OFFSET_BOOTSTRAP != 0
#error The createPackage() function is written under the assumption that the bootstrap
#error offset is 0, hence the library will not operate correctly with a different offset.
#endif
            for (int i = 0; i < LENGTH_BOOTSTRAP; i += 1)
            {
                nfd->write("\0", 1);
            }

            // Write out the INode lookup table.
            for (int i = 0; i < LENGTH_LOOKUP / 4; i += 1)
            {
                if (i == 0)
                {
                    uint32_t pos = OFFSET_DATA;
                    nfd->write(reinterpret_cast<char *>(&pos), 4);
                }
                else
                    nfd->write("\0\0\0\0", 4);
            }

            // Now add the FSInfo inode at OFFSET_FSINFO.
            INode fsnode(0, "", INodeType::INT_FSINFO);
            fsnode.ver_major = LIBRARY_VERSION_MAJOR;
            fsnode.ver_minor = LIBRARY_VERSION_MINOR;
            fsnode.ver_revision = LIBRARY_VERSION_REVISION;
            fsnode.setAppName(appname);
            fsnode.setAppVersion(appver);
            fsnode.setAppDesc(appdesc);
            fsnode.setAppAuthor(appauthor);
            fsnode.pos_root = OFFSET_DATA;
            fsnode.pos_freelist = 0; // The first FreeList block will automatically be
                         // created when the first block is freed.
            std::string fsnode_towrite = fsnode.getBinaryRepresentation();
            nfd->write(fsnode_towrite.c_str(), fsnode_towrite.size());
            for (int i = fsnode_towrite.size(); i < LENGTH_FSINFO; i += 1)
            {
                nfd->write("\0", 1);
            }

            time_t rtime;
            time(&rtime);

            // Now add the root inode at OFFSET_DATA.
            INode rnode(0, "", INodeType::INT_DIRECTORY);
            rnode.uid = 0;
            rnode.gid = 1000;
            rnode.mask = 0777;
            rnode.atime = rtime;
            rnode.mtime = rtime;
            rnode.ctime = rtime;
            rnode.parent = 0;
            rnode.children_count = 0;
            std::string rnode_towrite = rnode.getBinaryRepresentation();
            nfd->write(rnode_towrite.c_str(), rnode_towrite.size());
            for (int i = rnode_towrite.size(); i < BSIZE_FILE; i += 1)
            {
                nfd->write("\0", 1);
            }

            nfd->close();
            delete nfd;

            return true;
        }

        int Util::translateOpenMode(std::string mode)
        {
            if (mode == "r")
                return std::ios_base::in | std::ios_base::binary;
            else if (mode == "w")
                return std::ios_base::out | std::ios_base::binary;
            else if (mode == "rw")
                return std::ios_base::in | std::ios_base::out | std::ios_base::binary;
            else
                throw std::exception(); // FIXME: Appropriate message.
        }

        std::vector<std::string> Util::splitPathBySeperators(std::string path)
        {
            std::vector < std::string > ret;
            while (path[0] == '/' && path.length() > 0)
                path = path.substr(1);
            std::string buf = "";
            for (unsigned int i = 0; i < path.length(); i += 1)
            {
                if (path[i] == '/' && buf.length() > 0)
                {
                    ret.insert(ret.end(), buf);
                    buf = "";
                }
                else if (path[i] != '/')
                {
                    buf += path[i];
                }
            }
            if (buf.length() > 0)
            {
                ret.insert(ret.end(), buf);
                buf = "";
            }
            return ret;
        }

        FSResult::FSResult Util::verifyPath(std::string original, std::vector<std::string>& split)
        {
            if (original.length() >= 4096 || original.find('\0') != std::string::npos)
                return FSResult::E_FAILURE_INVALID_PATH;
            for (std::vector < std::string >::iterator i = split.begin(); i != split.end(); i++)
                if ((*i).length() >= 256 || (*i).find('\0') != std::string::npos)
                    return FSResult::E_FAILURE_INVALID_FILENAME;
            return FSResult::E_SUCCESS;
        }
    
        std::string Util::extractBasenameFromPath(std::string path)
        {
            std::vector<std::string> components = Util::splitPathBySeperators(path);
            if (components.size() == 0)
                return "";
            return components[components.size() - 1];
        }
    }
}
