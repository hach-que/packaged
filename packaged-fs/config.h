/* vim: set ts=4 sw=4 tw=0 et ai :*/

#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <argtable2.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <linux/limits.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <sys/mount.h>
#include <sys/signal.h>

/************************* BEGIN CONFIGURATION **************************/

// Uncomment the lines below to get different levels of verbosity.
// Note that ULTRAQUIET will prevent AppFS from showing *any* messages
// what-so-ever, including errors.
//
// #define DEBUGGING 1
// #define ULTRAQUIET 1

/************************** END CONFIGURATION ***************************/
