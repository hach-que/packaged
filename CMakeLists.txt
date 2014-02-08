# vim: set ts=4 sw=4 tw=0 ai et:
cmake_minimum_required(VERSION 2.8)
project(packaged)
set(CMAKE_MODULE_PATH ${packaged_SOURCE_DIR}/cmake)

# Configuration C++ paths.  Your C++ compiler MUST be
# able to support C++11.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_subdirectory(extern/argtable2)
add_subdirectory(libpackaged-fs)
add_subdirectory(packaged-fs)

