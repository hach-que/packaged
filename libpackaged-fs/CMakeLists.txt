# vim: set ts=4 sw=4 tw=0 ai et:
include_directories(${packaged_SOURCE_DIR})
add_library(packaged-fs STATIC
    lowlevel/endian.cpp
    lowlevel/inode.cpp
    lowlevel/fs.cpp
    lowlevel/freelist.cpp
    lowlevel/blockstream.cpp
    lowlevel/util.cpp
    internal/fuselink.cpp
    exception/package.cpp
    exception/fs.cpp
    exception/util.cpp
    environment.cpp
    logging.cpp
    fsfile.cpp
    fs.cpp
    )
find_package(FUSE REQUIRED)
add_definitions(-D_FILE_OFFSET_BITS=64)
include_directories(${FUSE_INCLUDE_DIRS})
target_link_libraries(packaged-fs ${FUSE_LIBRARIES})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
