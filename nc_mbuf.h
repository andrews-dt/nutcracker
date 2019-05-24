#ifndef _NC_MBUF_H_
#define _NC_MBUF_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <nc_util.h>
#include <nc_log.h>

#define MBUF_MAGIC      0xdeadbeef
#define MBUF_MIN_SIZE   512
#define MBUF_MAX_SIZE   16777216
#define MBUF_SIZE       16384
#define MBUF_HSIZE      sizeof(NcMbuf)

/*
* mbuf header is at the tail end of the mbuf. This enables us to catch
* buffer overrun early by asserting on the magic value during get or
* put operations
*
*   <------------- mbuf_chunk_size ------------->
*   +-------------------------------------------+
*   |       mbuf data          |  mbuf header   |
*   |     (mbuf_offset)        | (struct mbuf)  |
*   +-------------------------------------------+
*   ^           ^        ^     ^^
*   |           |        |     ||
*   \           |        |     |\
*   mbuf->start \        |     | mbuf->end (one byte past valid bound)
*                mbuf->pos     \
*                        \      mbuf
*                        mbuf->last (one byte past valid byte)
*
*/
class NcMbuf
{
public:
    NcMbuf()
    { 
        m_chunk_size_ = NcUtil::ncMbufChunkSize();
        m_data_ = (uint8_t *)nc_alloc(m_chunk_size_);
        m_start_ = m_data_;
        m_end_ = m_data_ + (m_chunk_size_ - MBUF_HSIZE);
        m_pos_ = m_start_;
        m_last_ = m_start_;
        m_magic_ = MBUF_MAGIC;
    }

    ~NcMbuf()
    {
        nc_free(m_data_);
    }

    inline void reset()
    {
        rewind();
    }

    inline bool empty()
    { 
        return m_pos_ == m_last_ ? true : false;
    }

    inline bool full()
    {
        return m_last_ == m_end_ ? true : false;
    }

    inline uint8_t* getPos()
    {
        return m_pos_;
    }

    inline uint8_t* getLast()
    {
        return m_last_;
    }

    inline void setPos(uint32_t n)
    {
        m_pos_ += n;
    }

    inline void setLast(uint32_t n)
    {
        m_last_ += n; 
    }

    inline void setComplete()
    {
        m_pos_ = m_last_;
    }

    inline void rewind()
    {
        m_pos_ = m_start_;
        m_last_ = m_end_;
    }

    inline uint32_t length()
    {
        return (uint32_t)(m_last_ - m_pos_);
    }

    inline uint32_t size()
    {
        return (uint32_t)(m_end_ - m_last_);
    }

    inline size_t dataSize()
    {
        return m_chunk_size_ - MBUF_HSIZE;
    }

    inline void copy(uint8_t *pos, size_t n)
    {
        if (n == 0)
        {
            return ;
        }

        nc_memcpy(m_last_, pos, n);
        m_last_ += n;
    }

    inline int vsnprintf(const char *fmt, ...)
    {
        va_list args;
        int n;
        uint32_t _size = size();

        va_start(args, fmt);
        n = nc_vsnprintf(m_last_, _size, fmt, args);
        va_end(args);

        if (n > 0)
        {
            m_last_ += n;
        }
        
        return n;
    }

    inline int vsnprintf(const char *fmt, va_list args)
    {
        int n;
        uint32_t _size = size();

        n = nc_vsnprintf(m_last_, _size, fmt, args);

        if (n > 0)
        {
            m_last_ += n;
        }
        
        return n;
    }
    
    // 分割mbuf的数据
    inline NcMbuf* split(uint8_t *pos)
    {
        FUNCTION_INTO(NcMbuf); 

        NcMbuf *nbuf = new NcMbuf();
        if (nbuf == NULL) 
        {
            LOG_DEBUG("nbuf is NULL, chunk_size : %d", m_chunk_size_);
            return NULL;
        }

        uint32_t _size = (size_t)(m_last_ - pos);
        LOG_DEBUG("m_last_ : %p, pos : %p, size : %d", m_last_, pos, _size);
        nbuf->copy(pos, _size);
        m_last_ = pos;

        return nbuf;
    }

private:
    uint32_t           m_magic_;   /* mbuf magic (const) */
    uint8_t            *m_pos_;    /* read marker */
    uint8_t            *m_last_;   /* write marker */
    uint8_t            *m_start_;  /* start of buffer (const) */
    uint8_t            *m_end_;    /* end of buffer (const) */
    uint8_t            *m_data_;
    uint32_t           m_chunk_size_; // 对应的size
};

#endif
