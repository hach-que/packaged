/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_UTIL
#define CLASS_UTIL

#include <libpackaged-fs/config.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <libpackaged-fs/lowlevel/fsresult.h>

namespace AppLib
{
    namespace LowLevel
    {
        class Util
        {
            public:
                static void seekp_ex(LowLevel::BlockStream * fd, std::streampos pos);
                static bool fileExists(std::string filename);
                static void sanitizeArguments(char ** argv, int argc, std::string & command, int start);
                static bool extractBootstrap(std::string source, std::string dest);
                static char* getProcessFilename();
                static bool createPackage(std::string path, const char* appname, const char* appver,
                            const char* appdesc, const char* appauthor);
                static int translateOpenMode(std::string mode);

                //! Utility function for splitting paths into their components.
                static std::vector<std::string> splitPathBySeperators(std::string path);

                //! Utility function for verifying the validity of a supplied path.
                static FSResult::FSResult verifyPath(std::string original, std::vector<std::string>& split);

                /*!
                 * Extracts the basename (filename) from the specified
                 * path.
                 */
                static std::string extractBasenameFromPath(std::string path);
        };
    }
}

#endif
