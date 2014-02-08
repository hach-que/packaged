/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/lowlevel/util.h>

int main(int argc, char *argv[])
{
    AppLib::Logging::setApplicationName("appcreate");
#ifdef DEBUG
    AppLib::Logging::debug = true;
#endif

    // Check arguments.
    if (argc != 2)
    {
        AppLib::Logging::showErrorW("Invalid arguments provided.");
        AppLib::Logging::showErrorO("Usage: appcreate <filename>");
        return 1;
    }

    std::cout << "Attempting to create '" << argv[1] << "' ... " << std::endl;

    // Create the file.
    if (!AppLib::LowLevel::Util::createPackage(argv[1], "Test Application", "1.0.0", "A test package.", "AppTools"))
    {
        std::cout << "Unable to create blank AppFS package '" << argv[1] << "'." << std::endl;
        return 1;
    }

    std::cout << "Package successfully created." << std::endl;
    return 0;
}
