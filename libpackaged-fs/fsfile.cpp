/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/fsfile.h>
#include <libpackaged-fs/lowlevel/fs.h>
#include <libpackaged-fs/lowlevel/util.h>
#include <libpackaged-fs/logging.h>
#include <libpackaged-fs/lowlevel/blockstream.h>
#include <map>
#include <math.h>
#include <stdarg.h>
#include <algorithm>

using namespace AppLib::LowLevel;

namespace AppLib
{
    FSFile::FSFile(FS * filesystem, BlockStream * fd, uint16_t inodeid)
    {
        this->inodeid = inodeid;
        this->filesystem = filesystem;
        this->fd = fd;
        this->opened = false;
        this->invalid = false;
        this->posg = 0;
        this->posp = 0;
        this->state = std::ios::goodbit;
    }

    void FSFile::open(std::ios_base::openmode mode)
    {
        if (this->bad() || this->fail())
            return;

        INode node = this->filesystem->getINodeByID(this->inodeid);
        this->invalid = (node.type != INodeType::INT_FILEINFO &&
                    node.type != INodeType::INT_SYMLINK);
        if (this->invalid)
        {
            this->clear(std::ios::badbit | std::ios::failbit);
            return;
        }

        // TODO: Check permissions of current user / group
        //       to see whether it can be opened.  If it fails
        //       to open, set the badbit on.
        this->opened = true;

        if (false /* <failed> */ )
        {
            this->clear(std::ios::badbit | std::ios::failbit);
        }
    }

    void FSFile::open(int mode)
    {
        // FIXME: This is a forced coercion since Cython can't
        // handle the STL's IO open modes.  This should really
        // reside in a stub file that is used just for Cython.
        this->open((std::ios_base::openmode)mode);
    }

    void FSFile::write(const char *data, std::streamsize count)
    {
        if (this->bad() || this->fail())
            return;

        if (this->invalid || !this->opened)
        {
            this->clear(std::ios::badbit | std::ios::failbit);
            return;
        }

        // Store the current positions.
        std::streampos oldg = this->fd->tellg();
        std::streampos oldp = this->fd->tellp();

        // Get the base position of the specified inode.
        uint32_t bpos = this->filesystem->getINodePositionByID(this->inodeid);

        // Get the total size of the file (for detected when to EOF).
        uint32_t fsize = this->size();

        // If we need to truncate the file to a new size, do so.
        if (fsize < this->posp + count)
        {
            if (!this->truncate(this->posp + count))
            {
                this->clear(std::ios::badbit | std::ios::failbit);
                return;
            }

            // Re-get the size.
            fsize = this->size();
        }

        // Calculate the number of blocks we will have to write.
        uint32_t bstart = (this->posp / 4096);
        uint32_t bend = ((this->posp + count - 1) / 4096);

        // Loop through all of the blocks in the file
        // First we get a list of all of the segments to write, starting
        // at this->posp until this->posp + count.
        uint32_t bcount = 0;
        uint32_t doff = 0;
        uint32_t spos = 0;
        uint32_t ipos = bpos;
        uint32_t hsize = HSIZE_FILE;
        while (ipos != 0)
        {
            for (int i = hsize; i < BSIZE_FILE; i += 4)
            {
                this->fd->seekg(bpos + i);
                spos = 0;
                Endian::doR(this->fd, reinterpret_cast < char *>(&spos), 4);

                if (fsize - this->posp == 0)
                {
                    // We've hit EOF.  Return.
                    this->fd->seekg(oldg);
                    this->fd->seekp(oldp);
                    this->clear(std::ios::eofbit);
                    return;
                }

                if (spos == 0)
                {
                    // We've run out of segments to write to (this shouldn't
                    // happen because we truncated the file).
                    ipos = 0;	// Make it jump out of the while() loop.
                    this->clear(std::ios::eofbit | std::ios::failbit);
                    return;
                }

                if (bcount < bstart)
                {
                    // Before the data needs to be read.
                    bcount += 1;
                    continue;
                }
                else if (bcount == bstart)
                {
                    // First block to read.  Calculate how many
                    // bytes to read (as it may not be the full block).
                    uint32_t soff = this->posp - ((this->posp / 4096) * 4096);

                    // Calculate how many bytes to read.
                    uint32_t stotal = std::min < uint32_t > (count - doff, std::min < uint32_t > (BSIZE_FILE - soff, fsize - this->posp));

                    // Seek the correct position.
                    this->fd->seekp(spos + soff);

                    // Write the selected number of bytes.
                    this->fd->write(data + doff, stotal);

                    // Increase the counters.
                    doff += stotal;
                    this->posp += stotal;

                    if (bcount == bend)
                    {
                        // We're going to finish writing on this block too.
                        this->fd->seekg(oldg);
                        this->fd->seekp(oldp);
                        if (this->posg == fsize)
                            this->clear(std::ios::eofbit);
                        return;
                    }
                }
                else if (bcount > bstart && bcount < bend)
                {
                    // A block in the middle which must be completely
                    // written to.

                    // Calculate how many bytes to write.
                    uint32_t stotal = std::min < uint32_t > (count - doff, std::min < uint32_t > ((uint32_t) BSIZE_FILE, fsize - this->posp));

                    // Seek the correct position.
                    this->fd->seekp(spos);

                    // Write the selected number of bytes.
                    this->fd->write(data + doff, stotal);

                    // Increase the counters.
                    doff += stotal;
                    this->posp += stotal;
                }
                else if (bcount > bstart && bcount == bend)
                {
                    // Last block to write to.  Calculate how many bytes
                    // to write in the last block.
                    uint32_t srem = count - doff;

                    // Calculate how many bytes to write.
                    uint32_t stotal = std::min < uint32_t > (srem, std::min < uint32_t > (count - doff, std::min < uint32_t > ((uint32_t) BSIZE_FILE, fsize - this->posp)));

                    // Seek the correct position.
                    this->fd->seekp(spos);

                    // Write the selected number of bytes.
                    this->fd->write(data + doff, stotal);

                    // Increase the counters.
                    doff += stotal;
                    this->posp += stotal;

                    // Now that we've reached the last block, we've
                    // written all the data and can now return.
                    this->fd->seekg(oldg);
                    this->fd->seekp(oldp);
                    if (this->posp == fsize)
                        this->clear(std::ios::eofbit);
                    return;
                }
                else
                {
                    // End of writing..  Return.
                    this->fd->seekg(oldg);
                    this->fd->seekp(oldp);
                    if (this->posp == fsize)
                        this->clear(std::ios::eofbit);
                    return;
                }

                bcount += 1;
            }
            hsize = HSIZE_SEGINFO;
            INode inode = this->filesystem->getINodeByPosition(ipos);
            ipos = inode.info_next;
        }

        // Set the fail bit if we get here, because we shouldn't.
        this->clear(std::ios::badbit | std::ios::failbit);
        return;
    }

    std::streamsize FSFile::read(char *out, std::streamsize count)
    {
        if (this->bad() || this->fail())
            return 0;

        if (this->invalid || !this->opened)
        {
            this->clear(std::ios::badbit | std::ios::failbit);
            return 0;
        }

        // Store the current positions.
        std::streampos oldg = this->fd->tellg();
        std::streampos oldp = this->fd->tellp();

        // Get the base position of the specified inode.
        uint32_t bpos = this->filesystem->getINodePositionByID(this->inodeid);

        // Calculate the number of blocks we will have to read.
        uint32_t bstart = (this->posg / 4096);
        uint32_t bend = ((this->posg + count - 1) / 4096);

        // Get the total size of the file (for detected when to EOF).
        uint32_t fsize = this->size();

        // First we get a list of all of the segments to read, starting
        // at this->posg until this->posg + count.
        uint32_t bcount = 0;
        uint32_t doff = 0;
        uint32_t spos = 0;
        uint32_t ipos = bpos;
        uint32_t hsize = HSIZE_FILE;
        while (ipos != 0)
        {
            for (int i = hsize; i < BSIZE_FILE; i += 4)
            {
                this->fd->seekg(bpos + i);
                spos = 0;
                Endian::doR(this->fd, reinterpret_cast < char *>(&spos), 4);

                if (fsize - this->posg == 0)
                {
                    // We've hit EOF.  Return.
                    this->fd->seekg(oldg);
                    this->clear(std::ios::eofbit);
                    return doff;
                }

                if (spos == 0)
                {
                    // We've run out of segments to read.
                    ipos = 0;	// Make it jump out of the while() loop.
                    this->clear(std::ios::eofbit);
                    return doff;
                }

                if (bcount < bstart)
                {
                    // Before the data needs to be read.
                    bcount += 1;
                    continue;
                }
                else if (bcount == bstart)
                {
                    // First block to read.  Calculate how many
                    // bytes to read (as it may not be the full block).
                    uint32_t soff = this->posg - ((this->posg / 4096) * 4096);

                    // Calculate how many bytes to read.
                    uint32_t stotal = std::min < uint32_t > (count - doff, std::min < uint32_t > ((uint32_t) BSIZE_FILE - soff, fsize - this->posg));

                    // Seek the correct position.
                    this->fd->seekg(spos + soff);

                    // Read the selected number of bytes.
                    uint32_t bread = this->fd->read(out + doff, stotal);

                    // Increase the counters.
                    if (this->posg + bread > fsize)
                    {
                        doff += fsize - this->posg;
                        this->posg = fsize;
                    }
                    else
                    {
                        doff += bread;
                        this->posg += bread;
                    }

                    if (bcount == bend)
                    {
                        // We're going to finish reading on this block too.
                        this->fd->seekg(oldg);
                        if (this->posg == fsize)
                            this->clear(std::ios::eofbit);
                        return doff;
                    }
                }
                else if (bcount > bstart && bcount < bend)
                {
                    // A block in the middle which must be completely
                    // read (up to EOF of course).

                    // Calculate how many bytes to read.
                    uint32_t stotal = std::min < uint32_t > (count - doff, std::min < uint32_t > ((uint32_t) BSIZE_FILE, fsize - this->posg));

                    // Seek the correct position.
                    this->fd->seekg(spos);

                    // Read the selected number of bytes.
                    uint32_t bread = this->fd->read(out + doff, stotal);

                    // Increase the counters.
                    if (this->posg + bread > fsize)
                    {
                        doff += fsize - this->posg;
                        this->posg = fsize;
                    }
                    else
                    {
                        doff += bread;
                        this->posg += bread;
                    }
                }
                else if (bcount > bstart && bcount == bend)
                {
                    // Last block to read.  Calculate how many bytes
                    // to read in the last block.
                    uint32_t srem = count - doff;

                    // Calculate how many bytes to read.
                    uint32_t stotal = std::min < uint32_t > (srem, std::min < uint32_t > (count - doff, std::min < uint32_t > ((uint32_t) BSIZE_FILE, fsize - this->posg)));

                    // Seek the correct position.
                    this->fd->seekg(spos);

                    // Read the selected number of bytes.
                    uint32_t bread = this->fd->read(out + doff, stotal);

                    // Increase the counters.
                    if (this->posg + bread > fsize)
                    {
                        doff += fsize - this->posg;
                        this->posg = fsize;
                    }
                    else
                    {
                        doff += bread;
                        this->posg += bread;
                    }

                    // Now that we've reached the last block, we've
                    // read all the data and can now return.
                    this->fd->seekg(oldg);
                    if (this->posg == fsize)
                        this->clear(std::ios::eofbit);
                    return doff;
                }
                else
                {
                    // End of reading..  Return.
                    this->fd->seekg(oldg);
                    if (this->posg == fsize)
                        this->clear(std::ios::eofbit);
                    return doff;
                }

                bcount += 1;
            }
            hsize = HSIZE_SEGINFO;
            INode inode = this->filesystem->getINodeByPosition(ipos);
            ipos = inode.info_next;
        }

        // If we get here, then there wasn't any blocks to read or the original
        // request to grab the inode based on ID failed, so we return 0 (for no
        // data read).
        return 0;
    }

    bool FSFile::truncate(std::streamsize len)
    {
        if (this->bad() || this->fail())
            return false;

        FSResult::FSResult fres = this->filesystem->truncateFile(this->inodeid, len);
        return (fres == FSResult::E_SUCCESS);
    }

    uint32_t FSFile::size()
    {
        INode fnode = this->filesystem->getINodeByID(this->inodeid);
        return fnode.dat_len;
    }

    void FSFile::close()
    {
        this->opened = false;
    }

    void FSFile::seekp(std::streampos pos)
    {
        if (this->bad() || this->fail())
            return;

        if (this->invalid || !this->opened)
        {
            this->clear(std::ios::badbit | std::ios::failbit);
            return;
        }

        this->posp = pos;
    }

    void FSFile::seekg(std::streampos pos)
    {
        if (this->bad() || this->fail())
            return;

        if (this->invalid || !this->opened)
        {
            this->clear(std::ios::badbit | std::ios::failbit);
            return;
        }

        this->posg = pos;
    }

    std::streampos FSFile::tellp()
    {
        return this->posp;
    }

    std::streampos FSFile::tellg()
    {
        return this->posg;
    }

    std::ios::iostate FSFile::rdstate()
    {
        return this->state;
    }

    void FSFile::clear()
    {
        this->state = std::ios::goodbit;
    }

    void FSFile::clear(std::ios::iostate state)
    {
        this->state = state;
    }

    bool FSFile::good()
    {
        return (this->state == std::ios::goodbit);
    }

    bool FSFile::bad()
    {
        return ((this->state & std::ios::badbit) != 0);
    }

    bool FSFile::eof()
    {
        return ((this->state & std::ios::eofbit) != 0);
    }

    bool FSFile::fail()
    {
        return ((this->state & std::ios::failbit) != 0);
    }
}
