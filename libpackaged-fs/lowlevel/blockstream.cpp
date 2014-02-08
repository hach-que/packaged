/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>

#include <string>
#include <iostream>
#include <libpackaged-fs/logging.h>
#include <libpackaged-fs/lowlevel/endian.h>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <errno.h>
#ifndef WIN32
#define _open ::open
#define _tell ::tell
#define _lseek ::lseek
#define _write ::write
#define _read ::read
#define _close ::close
#else
#include <io.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef WIN32
#define CREATE_CRITICAL() this->mutex = new pthread_mutex_t; pthread_mutex_init(this->mutex, NULL);
#define ENTER_CRITICAL() pthread_mutex_lock(this->mutex);
#define LEAVE_CRITICAL() pthread_mutex_unlock(this->mutex);
#else
#pragma message ("No support for critical locking in BlockStream on Windows yet.")
#define CREATE_CRITICAL() ;
#define ENTER_CRITICAL() ;
#define LEAVE_CRITICAL() ;
#endif

namespace AppLib
{
    namespace LowLevel
    {
        BlockStream::BlockStream(std::string filename)
        {
            CREATE_CRITICAL();

            ENTER_CRITICAL();

            this->opened = false;
            this->invalid = false;

            this->fd = new std::fstream(filename.c_str(), std::ios::in | std::ios::out | std::ios::binary);
            if (!this->fd->is_open())
            {
                Logging::showErrorW("Unable to open specified file as BlockStream.");
                this->invalid = true;
                this->opened = false;
                this->clear(std::ios::badbit | std::ios::failbit);
                return;
            }
            else
            {
                this->fd->exceptions(std::ifstream::badbit | std::ios::failbit | std::ios::eofbit);
                this->invalid = false;
                this->opened = true;
            }

            LEAVE_CRITICAL();
        }

        void BlockStream::write(const char *data, std::streamsize count)
        {
            ENTER_CRITICAL();

            if (this->invalid || !this->opened || this->fail())
            {
                if (!this->fail())
                    this->clear(std::ios::badbit | std::ios::failbit);
                return;
            }

            this->fd->write(data, count);

            LEAVE_CRITICAL();
        }

        std::streamsize BlockStream::read(char *out, std::streamsize count)
        {
            ENTER_CRITICAL();

            if (this->invalid || !this->opened || this->fail())
            {
                if (!this->fail())
                    this->clear(std::ios::badbit | std::ios::failbit);
                return 0;
            }

            this->fd->readsome(out, count);
            std::streamsize total = this->fd->gcount();

            LEAVE_CRITICAL();

            return total;
        }

        void BlockStream::close()
        {
            ENTER_CRITICAL();

            this->fd->close();
            this->opened = false;

            LEAVE_CRITICAL();
        }

        void BlockStream::seekp(std::streampos pos, std::ios_base::seekdir dir)
        {
            ENTER_CRITICAL();

            if (this->invalid || !this->opened || this->fail())
            {
                if (!this->fail())
                    this->clear(std::ios::badbit | std::ios::failbit);
                return;
            }

            this->fd->seekp(pos, dir);

            LEAVE_CRITICAL();
        }

        void BlockStream::seekg(std::streampos pos, std::ios_base::seekdir dir)
        {
            ENTER_CRITICAL();

            if (this->invalid || !this->opened || this->fail())
            {
                if (!this->fail())
                    this->clear(std::ios::badbit | std::ios::failbit);
                return;
            }

            this->fd->seekg(pos, dir);

            LEAVE_CRITICAL();
        }

        std::streampos BlockStream::tellp()
        {
            ENTER_CRITICAL();

            if (this->invalid || !this->opened || this->fail())
            {
                if (!this->fail())
                    this->clear(std::ios::badbit | std::ios::failbit);
                return 0;
            }

            std::streampos pos = this->fd->tellp();

            LEAVE_CRITICAL();

            return pos;
        }

        std::streampos BlockStream::tellg()
        {
            ENTER_CRITICAL();

            if (this->invalid || !this->opened || this->fail())
            {
                if (!this->fail())
                    this->clear(std::ios::badbit | std::ios::failbit);
                return 0;
            }

            std::streampos pos = this->fd->tellg();

            LEAVE_CRITICAL();

            return pos;
        }

        bool BlockStream::is_open()
        {
            return this->fd->is_open();
        }

        std::ios::iostate BlockStream::rdstate()
        {
            return this->fd->rdstate();
        }

        void BlockStream::clear()
        {
            this->fd->clear();
        }

        void BlockStream::clear(std::ios::iostate state)
        {
            this->fd->clear(state);
        }

        bool BlockStream::good()
        {
            return this->fd->good();
        }

        bool BlockStream::bad()
        {
            return this->fd->bad();
        }

        bool BlockStream::eof()
        {
            return this->fd->eof();
        }

        bool BlockStream::fail()
        {
            return this->fd->fail();
        }
    }
}
