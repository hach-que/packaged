# vim: set ts=4 sw=4 tw=0 ai et:
include_directories(${packaged_SOURCE_DIR})
include_directories(${packaged_SOURCE_DIR}/extern/argtable2)

add_executable(packaged-fsbootstrap appfs.cpp)
add_executable(packaged-fsmount appmount.cpp)
add_executable(packaged-fscreate appcreate.cpp)
add_executable(packaged-fsinspect appinspect.cpp)
target_link_libraries(packaged-fsbootstrap packaged-fs argtable2 pthread)
target_link_libraries(packaged-fsmount packaged-fs argtable2)
target_link_libraries(packaged-fscreate packaged-fs argtable2)
target_link_libraries(packaged-fsinspect packaged-fs argtable2)
add_definitions("-D_FILE_OFFSET_BITS=64")
