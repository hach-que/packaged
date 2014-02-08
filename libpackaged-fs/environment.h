/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_ENVIRONMENT
#define CLASS_ENVIRONMENT

#include <libpackaged-fs/config.h>
#include <string>
#include <vector>

namespace AppLib
{
    class Environment
    {
        public:
            static std::vector<bool> searchForBinaries(std::vector<std::string> & binaries);
    };
}

#endif
