/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>

#include <string>
#include <iostream>
#include <fstream>
#include <libpackaged-fs/lowlevel/endian.h>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <libpackaged-fs/logging.h>

namespace AppLib
{
    namespace LowLevel
    {
        bool Endian::little_endian = true;

        void Endian::detectEndianness()
        {
            uint16_t test_int = 1;
            char *test_int_raw = reinterpret_cast < char *>(&test_int);
             Endian::little_endian = (test_int_raw[0] == 1);
        }

        void Endian::doR(BlockStream * fd, char *data, unsigned int size)
        {
            if (Endian::little_endian)
                fd->read(data, size);
            else
            {
                char *dStorage = (char *) malloc(size);
                 fd->read(dStorage, size);
                for (unsigned int i = 0; i < size; i += 1)
                {
                    data[i] = dStorage[size - i];
                }
            }
            if (fd->fail() && fd->eof())
                 fd->clear(std::ios::eofbit);
            else if (fd->fail() && fd->bad())
            {
                Logging::showErrorW("I/O error occurred while reading from file.");
                fd->clear();
            }
            else if (fd->fail())
            {
                Logging::showWarningW("Unexpected failure while reading from file.");
                fd->clear();
            }
        }

        void Endian::doW(BlockStream * fd, char *data, unsigned int size)
        {
            if (Endian::little_endian)
                fd->write(data, size);
            else
            {
                char *dStorage = (char *) malloc(size);
                for (unsigned int i = 0; i < size; i += 1)
                {
                    dStorage[size - i] = data[i];
                }
                fd->write(dStorage, size);
            }
            if (fd->fail() && fd->eof())
                fd->clear(std::ios::eofbit);
            else if (fd->fail() && fd->bad())
            {
                Logging::showErrorW("I/O error occurred while writing to file.");
                fd->clear();
            }
            else if (fd->fail())
            {
                Logging::showWarningW("Unexpected failure while writing to file.");
                fd->clear();
            }
        }

        void Endian::doW(BlockStream * fd, const char *data, unsigned int size)
        {
            Endian::doW(fd, const_cast < char *>(data), size);
        }

        void Endian::doR(std::iostream * fd, char *data, unsigned int size)
        {
            if (Endian::little_endian)
                fd->read(data, size);
            else
            {
                char *dStorage = (char *) malloc(size);
                fd->read(dStorage, size);
                for (unsigned int i = 0; i < size; i += 1)
                {
                    data[i] = dStorage[size - i];
                }
            }
            if (fd->fail() && fd->eof())
                fd->clear(std::ios::eofbit);
            else if (fd->fail() && fd->bad())
            {
                Logging::showErrorW("I/O error occurred while reading from file.");
                fd->clear();
            }
            else if (fd->fail())
            {
                Logging::showWarningW("Unexpected failure while reading from file.");
                fd->clear();
            }
        }

        void Endian::doW(std::iostream * fd, char *data, unsigned int size)
        {
            if (Endian::little_endian)
                fd->write(data, size);
            else
            {
                char *dStorage = (char *) malloc(size);
                for (unsigned int i = 0; i < size; i += 1)
                {
                    dStorage[size - i] = data[i];
                }
                fd->write(dStorage, size);
            }
            if (fd->fail() && fd->eof())
                fd->clear(std::ios::eofbit);
            else if (fd->fail() && fd->bad())
            {
                Logging::showErrorW("I/O error occurred while writing to file.");
                fd->clear();
            }
            else if (fd->fail())
            {
                Logging::showWarningW("Unexpected failure while writing to file.");
                fd->clear();
            }
        }

        void Endian::doW(std::iostream * fd, const char *data, unsigned int size)
        {
            Endian::doW(fd, const_cast < char *>(data), size);
        }
    }
}
