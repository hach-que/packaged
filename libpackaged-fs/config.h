/* vim: set ts=4 sw=4 tw=0 et ai :*/

#define FUSE_USE_VERSION 26

#ifndef APPFS_CONFIG
#define APPFS_CONFIG

/*********** Start Configuration *************/
// Defines the offsets and lengths used for different
// sections within AppFS packages.  You should not change
// these for compatibility reasons.
#define OFFSET_BOOTSTRAP 0
#define LENGTH_BOOTSTRAP (3 * 1024 * 1024)
#define OFFSET_LOOKUP    LENGTH_BOOTSTRAP
#define LENGTH_LOOKUP    (256 * 1024)
#define OFFSET_FSINFO    (LENGTH_BOOTSTRAP + LENGTH_LOOKUP)
#define LENGTH_FSINFO    (4096)
#define OFFSET_DATA      (LENGTH_BOOTSTRAP + LENGTH_LOOKUP + LENGTH_FSINFO)

// Name of the filesystem implementation.  Must be 9 characters
// because the automatic terminating NULL character makes it 10
// in total (and we write out 10 bytes to our FSINFO block).
#define FS_NAME "AppFS\0\0\0\0"

// Defines whether or not the test suite should automatically
// create a blank AppFS file for testing if it doesn't exist
// in the local directory.
#define TESTSUITE_AUTOCREATE 1

// Define the raw block sizes.  The directory block size must
// be a multiple of the file block size.
#define BSIZE_FILE      4096
#define BSIZE_DIRECTORY 4096

// Number of subdirectories / subfiles allowed in a single
// directory.
#define DIRECTORY_CHILDREN_MAX 1901

// The maximum file size allowed (10MB + data offset from the 32-bit
// integer limit).
#define MSIZE_FILE (0xFFFFFFFF - OFFSET_DATA - (1024 * 1024 * 10))

// Define the sizes of each of the header types.
#define HSIZE_FILE       308
#define HSIZE_SEGINFO    8
#define HSIZE_FREELIST   8
#define HSIZE_FSINFO     1614
#define HSIZE_DIRECTORY  294

/************ End Configuration **************/

#define LIBRARY_VERSION_MAJOR 0
#define LIBRARY_VERSION_MINOR 1
#define LIBRARY_VERSION_REVISION 0

#if defined(_MSC_VER)
typedef signed __int8 int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef signed __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
typedef unsigned int mode_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int pid_t;
#define ENOTSUP 95
#define EALREADY 114
#else
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __cpluscplus
#include <cstring>
#else
#include <string.h>
#endif
#endif

// We need 64-bit time support.  Since it seems time.h has no
// _time64 function in UNIX,  we are probably going to have to
// statically reference it from an external library.  This macro
// ensures that all of the code will switch over to use the new
// function when such a function is located.  For now, it uses
// time() in time.h.
#define APPFS_TIME() time(NULL);

#endif
