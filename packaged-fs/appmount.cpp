/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/logging.h>
#include <libpackaged-fs/internal/fuselink.h>
#include "config.h"
#include "funcdefs.h"

std::string global_mount_path = "<not set>";

int appmount_start(int argc, char *argv[])
{
    const char *disk_path = NULL;
    const char *mount_path = NULL;

    // Set the application name.
    AppLib::Logging::setApplicationName(std::string("appmount"));

    // Parse the arguments provided.
    struct arg_lit *is_readonly = arg_lit0("r", "read-only", "mount the file readonly");
    struct arg_lit *is_debug = arg_lit0("d", "debug", "show debugging information");
    struct arg_lit *is_allow_other = arg_lit0("o", "allow-other", "allow other users to access mounted application");
    struct arg_file *disk_image = arg_file1(NULL, NULL, "diskimage", "the image to read the data from");
    struct arg_file *mount_point = arg_file1(NULL, NULL, "mountpoint", "the directory to mount the image to");
    struct arg_lit *show_help = arg_lit0("h", "help", "show the help message");
    struct arg_end *end = arg_end(20);
#ifdef DEBUG
    void *argtable[] = { is_debug, is_allow_other, disk_image, mount_point, show_help, end };
#else
    void *argtable[] = { is_allow_other, disk_image, mount_point, show_help, end };
#endif

    // Check to see if the argument definitions were allocated
    // correctly.
    if (arg_nullcheck(argtable))
    {	
        AppLib::Logging::showErrorW("Insufficient memory.");
        return 1;
    }

    // Now parse the arguments.
    int nerrors = arg_parse(argc, argv, argtable);

    // Check to see if there were errors.
    if (nerrors > 0 && show_help->count == 0)
    {
        printf("Usage: appmount");
        arg_print_syntax(stdout, argtable, "\n");

        arg_print_errors(stdout, end, "appfs");
        return 1;
    }

    // Check to see if the user requested showing the help
    // message.
    if (show_help->count == 1)
    {
        printf("Usage: appmount");
        arg_print_syntax(stdout, argtable, "\n");

        printf("AppFS - An application storage filesystem.\n\n");
        arg_print_glossary(stdout, argtable, "    %-25s %s\n");
        return 0;
    }

    // Store the disk path and mount point the user has provided.
    disk_path = disk_image->filename[0];
    mount_path = mount_point->filename[0];
    global_mount_path = mount_path;
    AppLib::Logging::debug = is_debug->count;

    // Open the file for our lock checks / sets.
    /*int lockedfd = open(disk_image->filename[0], O_RDWR);
     * bool locksuccess = true;
     * flock lock = { F_RDLCK, SEEK_SET, 0, 0, 0 };
     * 
     * // Check to see whether there is a lock on the file already.
     * int lockres = fcntl(lockedfd, F_GETLK, &lock);
     * if (lockres == -1)
     * locksuccess = false;
     * if (locksuccess)
     * if (lock.l_type != F_UNLCK)
     * locksuccess = false;
     * if (!locksuccess)
     * {
     * AppLib::Logging::showErrorW("Unable to lock image.  Check to make sure it's");
     * AppLib::Logging::showErrorO("not already mounted.");
     * return 1;
     * } */

    // Lock the specified disk file.
    /*lock = { F_WRLCK, SEEK_SET, 0, 0, 0 };
     * lockres = fcntl(lockedfd, F_SETLK, &lock);
     * int ret = 1;
     * if (lockres != -1)
     * { */
    AppLib::Logging::showInfoW("The application package will now be mounted at:");
    AppLib::Logging::showInfoO("  * %s", global_mount_path.c_str());
    AppLib::Logging::showInfoO("You can use fusermount (or umount if root) to unmount the");
    AppLib::Logging::showInfoO("application package.  Please note that the package is locked");
    AppLib::Logging::showInfoO("while mounted and that no other operations can be performed");
    AppLib::Logging::showInfoO("on it while this is the case.");

    AppLib::FUSE::Mounter * mnt = new AppLib::FUSE::Mounter(disk_path, mount_path, true, is_allow_other->count, appmount_continue);
    int ret = mnt->getResult();

    if (ret != 0)
    {
        // Mount failed.
        AppLib::Logging::showErrorW("FUSE was unable to mount the application package.");
        AppLib::Logging::showErrorO("Check that the package is a valid AppFS filesystem and");
        AppLib::Logging::showErrorO("run 'apputil check' to scan for filesystem errors.");
    }
    else
    {
        // Unlock the file.
        // TODO: Find out documentation on unlocking files.
    }

    return ret;
    /*}
     * else
     * {
     * AppLib::Logging::showErrorW("Unable to lock image.  Check to make sure it's");
     * AppLib::Logging::showErrorO("not already mounted.");
     * return 1;
     * } */

    return 0;
}

void appmount_continue()
{
    // Execution continues at this point when the filesystem is mounted.
}

int main(int argc, char *argv[])
{
    return appmount_start(argc, argv);
}
