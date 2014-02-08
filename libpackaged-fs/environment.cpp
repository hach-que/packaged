/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>
#include <libpackaged-fs/lowlevel/util.h>
#include <libpackaged-fs/environment.h>
#include <string>
#include <vector>

namespace AppLib
{
    std::vector<bool> Environment::searchForBinaries(std::vector<std::string> & binaries)
    {
        std::vector<std::string> * path_list = new std::vector<std::string>();
        char * env_path;
                env_path = getenv("PATH");
                if (env_path != NULL)
                {
                        // Seperate the path directories by the : character.
                        std::string buffer = "";
                        for (int s = 0; s < strlen(env_path); s++)
                        {
                                if (env_path[s] == ':')
                                {
                                        // seperate.
                                        path_list->insert(path_list->end(), std::string(buffer));
                                        buffer = "";
                                }
                                else
                                {
                                        buffer += env_path[s];
                                }
                        }
                        if (buffer != "")
                        {
                                path_list->insert(path_list->end(), std::string(buffer));
                                buffer = "";
                        }

                        // Now each of our paths is inside path_list.  We're going to iterate over them
                        // to search for each of the executables.
            std::vector<bool> found_results;
                        for (std::vector<std::string>::iterator bit = binaries.begin(); bit < binaries.end(); bit++)
                        {
                bool res = false;
                for (std::vector<std::string>::iterator dit = path_list->begin(); dit < path_list->end(); dit++)
                {
                    std::string bpath = *dit;
                    bpath += "/";
                    bpath += *bit;
                    if (LowLevel::Util::fileExists(bpath))
                    {
                        res = true;
                        break;
                    }
                }
                found_results.insert(found_results.end(), res);
                        }
            return found_results;
        }
        else
        {
            std::vector<bool> failres;
            for (int i = 0; i < binaries.size(); i += 1)
                failres.insert(failres.end(), false);
            return failres;
        }
    }
}

