/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/logging.h>
#include <libpackaged-fs/lowlevel/util.h>
#include <libpackaged-fs/internal/fuselink.h>
#include <libpackaged-fs/environment.h>
#include "config.h"
#include "funcdefs.h"
#include <pthread.h>
#include <string>
#include <libgen.h>
#include <stdexcept>

#define APPFS_BOOTSTRAP_VERSION_STR "0_1_0"
#define APPFS_BOOTSTRAP_VERSION_NUM "0.1.0"

struct appfs_thread_info
{
    const char *mount_path;
    const char *disk_path;
    int argc;
    char **argv;
};

// I don't like using global variables, but I see
// no other, short way of passing this information
// around other than modifing the parameters taken
// by the continuation function.
std::string global_disk_path = "";
std::string global_mount_path = "";
int global_argc = 0;
char **global_argv = NULL;

int appfs_stage1(int argc, char *argv[])
{
    // This stage is called when the bootstrap is still attached to a 
    // a filesystem.  Since we can't write to a file that's currently
    // being executed and we don't want to create race conditions by
    // unlink'ing the main executable and recreating it, we're going to
    // copy the first 1MB out of argv[0] (the bootstrap component) to
    // a temporary directory, and we will then execute that.  Once stage1
    // has quit, the bootstrap will then begin stage2 using the original
    // package.
#ifdef DEBUG
    AppLib::Logging::debug = true;
#endif
    AppLib::Logging::setApplicationName(std::string("appfs"));

    // Create a temporary directory for storing the bootstrap.  We will name
    // the extracted file "appfs_0_1_0" (based on APPFS_BOOTSTRAP_VERSION), so
    // that in future we will be able to do in-place updates of the bootstrap.
    char stage2_path[] = "/tmp/appfs_stage2.XXXXXX";
    if (mkdtemp(stage2_path) == NULL)
    {
        AppLib::Logging::showErrorW("Unable to create temporary directory for stage 2 of execution.");
        return 1;
    }
    std::string final_path = stage2_path;
    final_path += "/appfs_";
    final_path += APPFS_BOOTSTRAP_VERSION_STR;

    if (!AppLib::LowLevel::Util::extractBootstrap(argv[0], final_path))
    {
        AppLib::Logging::showErrorW("Unable to extract bootstrap component to temporary directory.");
        return 1;
    }

    // Make the new bootstrap file executable.
    if (chmod(final_path.c_str(), 0700) != 0)
    {
        AppLib::Logging::showErrorW("Unable to mark new bootstrap application as executable.");
        return 1;
    }

    // Execute the temporary bootstrap.  After execv, the current process image
    // will be replaced with the new one.
    if (execv(final_path.c_str(), argv) == -1)
    {
        AppLib::Logging::showErrorW("Unable to initiate stage 2 of bootstrap execution.");
        AppLib::Logging::showErrorO("The errno value is %i", errno);
        return 1;
    }

    return 0;
}

int appfs_stage2(int argc, char *argv[])
{
    // Set the application name.
#ifdef DEBUG
    AppLib::Logging::verbose = true;
#endif
    AppLib::Logging::setApplicationName(std::string("appfs"));

    // Set global variables.
    global_argc = argc;
    global_argv = argv;

    // AppFS needs a temporary location for the mountpoint.
    char mount_path_template[] = "/tmp/appfs_mount.XXXXXX";
    char *mount_tmp = mkdtemp(mount_path_template);
    if (mount_tmp == NULL)
    {
        AppLib::Logging::showErrorW("Unable to create temporary directory for mounting.");
        return 1;
    }
    global_mount_path = mount_tmp;

    // Now work out the absolute path to the disk image.
    global_disk_path = "";
    char *cwdbuf = (char *) malloc(PATH_MAX + 1);
    for (int i = 0; i < PATH_MAX + 1; i += 1)
        cwdbuf[i] = 0;
    if (getcwd(cwdbuf, PATH_MAX) == NULL)
    {
        AppLib::Logging::showErrorW("Unable to retrieve current working directory.");
        return 1;
    }
    global_disk_path += cwdbuf;
    global_disk_path += "/";
    global_disk_path += argv[0];

    // Now mount and run the application.
    AppLib::FUSE::Mounter * mnt = new AppLib::FUSE::Mounter(global_disk_path.c_str(), global_mount_path.c_str(), true, false, appfs_continue);
    int ret = mnt->getResult();

    if (ret != 0)
    {
        // Mount failed.
        AppLib::Logging::showErrorW("FUSE was unable to mount the application package.");
        AppLib::Logging::showErrorO("Check that the package is a valid AppFS filesystem and");
        AppLib::Logging::showErrorO("run 'apputil check' to scan for filesystem errors.");
        return ret;
    }

    // Remove our temporary directory since we are now unmounted.
    if (rmdir(global_mount_path.c_str()) != 0)
    {
        AppLib::Logging::showWarningW("Unable to delete temporary mountpoint directory.  You may");
        AppLib::Logging::showWarningO("have to clean it up yourself by deleting:");
        AppLib::Logging::showWarningO(" * %s", global_mount_path.c_str());
    }

    return 0;
}

void *appfs_thread(void *ptr);

void appfs_continue()
{
    // Execution continues at this point when the filesystem is mounted.
    // We're going to create another thread in which to house the execution
    // of the /EntryPoint script.
    pthread_t thread;
    appfs_thread_info *thread_info = new appfs_thread_info();
    thread_info->disk_path = global_disk_path.c_str();
    thread_info->mount_path = global_mount_path.c_str();
    thread_info->argc = global_argc;
    thread_info->argv = global_argv;
    int iret = pthread_create(&thread, NULL, appfs_thread, (void *) thread_info);
    // Caution: The thread will delete thread_info from memory.
}

void *appfs_thread(void *ptr)
{
    appfs_thread_info *thread_info = (appfs_thread_info *) ptr;
    std::string disk_path = thread_info->disk_path;
    std::string mount_path = thread_info->mount_path;
    int argc = thread_info->argc;
    char **argv = thread_info->argv;
    delete thread_info;

    // This function is where we will execute our /EntryPoint script as
    // we need to leave FUSE running in the background.
    std::string command = mount_path + "/EntryPoint";
    if (AppLib::LowLevel::Util::fileExists(command.c_str()))
    {
        // We're going to start the application and wait for
        // execution to finish.
        AppLib::LowLevel::Util::sanitizeArguments(argv, argc, command, 1);

        // Make a new directory to hold our sandbox mount point.
        bool inside_sandbox = true;
        bool alternative_sandbox = false;
        bool should_run = true;
        char sandbox_mount_path[] = "/tmp/appfs_sandbox.XXXXXX";
        if (mkdtemp(sandbox_mount_path) == NULL)
        {
            AppLib::Logging::showErrorW("Unable to sandbox application (unable to create temporary directory).");
            inside_sandbox = false;
            should_run = false;
        }
        AppLib::Logging::showInfoW("Created sandboxing directory at: %s", sandbox_mount_path);

        // Check to see if "unionfs-fuse", "uchroot" and "fusermount" are available in the PATH.
        // TODO: AppLib::Environment::searchForBinaries function needs to be implemented before
        //       this check will work.
        std::vector < std::string > search_apps;
        search_apps.insert(search_apps.end(), "unionfs-fuse");
        search_apps.insert(search_apps.end(), "uchroot");
        search_apps.insert(search_apps.end(), "fakechroot");
        search_apps.insert(search_apps.end(), "chroot");
        search_apps.insert(search_apps.end(), "fusermount");
        std::vector < bool > search_result = AppLib::Environment::searchForBinaries(search_apps);
        bool found_sandboxer = search_result[0];
        bool found_chrooter = search_result[1] || (search_result[2] && search_result[3]);
        bool found_unmounter = search_result[4];
        alternative_sandbox = (!search_result[1] && found_chrooter);
        if (!found_sandboxer || !found_chrooter || !found_unmounter)
            inside_sandbox = false;

        // Now run the application, or exit (depending on the status of inside_sandbox,
        // alternative_sandbox and should_run).
        if (should_run && inside_sandbox)
        {
            AppLib::Logging::showInfoW("Setting up sandbox (via unionfs-fuse)...");
            std::string sandbox_command = "unionfs-fuse -o cow,max_files=32768,allow_other,use_ino,suid,dev,nonempty ";
            sandbox_command += mount_path;
            sandbox_command += "=RW:/=RO ";
            sandbox_command += sandbox_mount_path;
            system(sandbox_command.c_str());

            // Now that the sandbox is setup, wrap the command in a chroot environment.
            std::string build;
            if (!alternative_sandbox)
                build = "uchroot ";
            else
                build = "fakechroot chroot ";
            build += sandbox_mount_path;
            build += " ";
            build += command;
            command = build;

            // Run the command.
            char *old_cwd = getcwd(NULL, 0);
            chdir(mount_path.c_str());
            system(command.c_str());
            chdir(old_cwd);

            // Now we need to close the temporary sandbox mountpoint
            // using fusermount.
            std::string fusermount_command = "fusermount -u ";
            fusermount_command += sandbox_mount_path;
            fusermount_command += ">/dev/null 2>/dev/null";
            int ret = -1;
            int count_tries = 0;
            while (ret != 0 && count_tries < 10)
            {
                ret = system(fusermount_command.c_str());
                if (ret != 0)
                {
                    sleep(1);
                    count_tries += 1;
                }
            }
            if (ret != 0)
            {
                AppLib::Logging::showErrorW("Unable to cleanup sandbox (mount point still in use after 10 attempts)");
                AppLib::Logging::showErrorO("You may have to manually unmount the sandbox and remove the");
                AppLib::Logging::showErrorO("mountpoint manually.  The mountpoint is:");
                AppLib::Logging::showErrorO(" * %s", sandbox_mount_path);
                kill(getpid(), SIGHUP);
                return NULL;
            }
        }
        else
        {
            AppLib::Logging::showErrorW("Sandboxing prerequisites not found.  One or more of the following applications:");
            AppLib::Logging::showErrorO(" * uchroot (or fakechroot AND chroot)");
            AppLib::Logging::showErrorO(" * unionfs-fuse");
            AppLib::Logging::showErrorO(" * fusermount");
            AppLib::Logging::showErrorO("were not found in PATH.  Since they are not available on the system");
            AppLib::Logging::showErrorO("you must install the application system-wide to run it.");
        }

        // Remove the sandbox directory.
        if (rmdir(sandbox_mount_path) != 0)
        {
            AppLib::Logging::showErrorW("Unable to cleanup sandbox (unable to delete temporary directory)");
            AppLib::Logging::showErrorO("You may have to manually remove the mountpoint.  The path to the");
            AppLib::Logging::showErrorO("mountpoint is:");
            AppLib::Logging::showErrorO(" * %s", sandbox_mount_path);
        }

        // Send SIGHUP to the parent process to instruct
        // FUSE to exit.
        kill(getpid(), SIGHUP);
        return NULL;
    }
    else
    {
        // The entry point does not exist.  Notify the user they
        // should use AppMount to mount the image (though in future
        // we will be installing the package here, so that packages
        // can be installed regardless of whether or not the client
        // has AppTools installed).
        AppLib::Logging::showErrorW("No /EntryPoint found in this application package.  Use AppMount");
        AppLib::Logging::showErrorO("to create one.");

        // Send SIGHUP to the parent process to instruct
        // FUSE to exit.
        kill(getpid(), SIGHUP);
        return NULL;
    }
}

int main(int argc, char *argv[])
{
    // Determine whether or not we're starting stage 1 or stage 2.  This
    // depends on whether or not the filesize is less than or equal to
    // 1MB (in which case it'll be stage 2).
    struct stat scheck;
    const char *exepath = AppLib::LowLevel::Util::getProcessFilename();
    AppLib::Logging::setApplicationName("appfs");	// Just so our error messages
    // have better context than 'apptools'.
    if (exepath == NULL)
    {
        AppLib::Logging::showErrorW("Unable to determine process's filename on this system.");
        AppLib::Logging::showErrorO("If you are running Linux, make sure /proc is mounted");
        AppLib::Logging::showErrorO("and that /proc/self/exe exists.");
        return 1;
    }
    if (stat(exepath, &scheck) < 0)
    {
        AppLib::Logging::showErrorW("Unable to detect size of bootstrap application.");
        return 1;
    }
    if (scheck.st_size <= 1024 * 1024)
    {
        // Stage 2.

        // First unlink ourselves from the filesystem.
        if (unlink(exepath) != 0)
        {
            AppLib::Logging::showWarningW("Unable to delete temporary bootstrap file.  You may");
            AppLib::Logging::showWarningO("have to clean it up yourself by deleting:");
            AppLib::Logging::showWarningO(" * %s", exepath);
        }

        // Then remove the directory that contains us (if it begins with appfs_stage2).
        int plen = strlen(exepath);
        char *drpath = (char *) malloc(plen + 1);
        for (int i = 0; i < plen + 1; i += 1)
            drpath[i] = 0;
        strcpy(drpath, exepath);
        dirname(drpath);
        std::string ddrpath = drpath;
        try
        {
            if (ddrpath.substr(ddrpath.length() - 20, 14) == "/appfs_stage2.")
            {
                if (rmdir(drpath) != 0)
                {
                    AppLib::Logging::showWarningW("Unable to delete temporary bootstrap directory.  You may");
                    AppLib::Logging::showWarningO("have to clean it up yourself by deleting:");
                    AppLib::Logging::showWarningO(" * %s", drpath);
                }
            }
        }
        catch(std::out_of_range & err)
        {
            // Nothing to do...
        }

        return appfs_stage2(argc, argv);
    }
    else
    {
        // Stage 1.
        return appfs_stage1(argc, argv);
    }
}
