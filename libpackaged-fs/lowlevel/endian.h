/* vim: set ts=4 sw=4 tw=0 et ai :*/

#ifndef CLASS_ENDIAN
#define CLASS_ENDIAN

#include <libpackaged-fs/config.h>

#include <string>
#include <iostream>
#include <fstream>

namespace AppLib
{
    namespace LowLevel
    {
        class BlockStream;

        class Endian
        {
            public:
                static bool little_endian;
                static void detectEndianness();
                static void doR(BlockStream * fd, char * data, unsigned int size);
                static void doW(BlockStream * fd, char * data, unsigned int size);
                static void doW(BlockStream * fd, const char * data, unsigned int size);
                static void doR(std::iostream * fd, char * data, unsigned int size);
                static void doW(std::iostream * fd, char * data, unsigned int size);
                static void doW(std::iostream * fd, const char * data, unsigned int size);
        };
    }
}

#endif
