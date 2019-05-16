#ifndef _NC_STRING_H_
#define _NC_STRING_H_

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

#define nc_memcpy(_d, _c, _n)               memcpy(_d, _c, (size_t)(_n))

#define nc_memmove(_d, _c, _n)              memmove(_d, _c, (size_t)(_n))

#define nc_memchr(_d, _c, _n)               memchr(_d, _c, (size_t)(_n))

#define nc_strlen(_s)                       strlen((char *)(_s))

#define nc_strncmp(_s1, _s2, _n)            strncmp((char *)(_s1), (char *)(_s2), (size_t)(_n))

#define nc_strchr(_p, _l, _c)               NcString::_nc_strchr((uint8_t *)(_p), (uint8_t *)(_l), (uint8_t)(_c))

#define nc_strrchr(_p, _s, _c)              NcString::_nc_strrchr((uint8_t *)(_p),(uint8_t *)(_s), (uint8_t)(_c))

#define nc_strndup(_s, _n)                  (uint8_t *)strndup((char *)(_s), (size_t)(_n))

#define nc_snprintf(_s, _n, ...)            snprintf((char *)(_s), (size_t)(_n), __VA_ARGS__)

#define nc_scnprintf(_s, _n, ...)           _scnprintf((char *)(_s), (size_t)(_n), __VA_ARGS__)

#define nc_vsnprintf(_s, _n, _f, _a)        vsnprintf((char *)(_s), (size_t)(_n), _f, _a)

#define nc_vscnprintf(_s, _n, _f, _a)       _vscnprintf((char *)(_s), (size_t)(_n), _f, _a)

#define nc_strftime(_s, _n, fmt, tm)        (int)strftime((char *)(_s), (size_t)(_n), fmt, tm)

#define nc_safe_snprintf(_s, _n, ...)       _safe_snprintf((char *)(_s), (size_t)(_n), __VA_ARGS__)

#define nc_safe_vsnprintf(_s, _n, _f, _a)   _safe_vsnprintf((char *)(_s), (size_t)(_n), _f, _a)

class NcString
{
public:
    NcString()
    {
        m_len_ = 0;
        // 初始化一个值
        m_data_ = (uint8_t*)malloc(sizeof(uint8_t));
        m_data_[0] = '\0';
    }

    NcString(const NcString &s)
    {
        m_data_ = nc_strndup(s.c_str(), s.length() + 1);
        if (m_data_ != NULL) 
        {
            m_len_ = s.length();
            m_data_[m_len_] = '\0';
        }
    }

    NcString(const uint8_t *s, uint32_t len)
    {
        if (m_data_ == s)
        {
            return ;
        }

        m_len_ = len;
        m_data_ = nc_strndup(s, m_len_ + 1);
        if (m_data_ != NULL) 
        {
            m_data_[m_len_] = '\0';
        }
        else
        {
            m_len_ = 0;
        }
        
        return ;
    }

    ~NcString()
    {
        if (m_data_ != NULL)
        {
            free(m_data_);
        }

        m_data_ = NULL;
    }

    NcString& operator=(const NcString &s)
    {
        if (this == &s)
        {
            return *this;
        }

        uint8_t *temp = m_data_;
        m_data_ = nc_strndup(s.c_str(), s.length() + 1);
        if (m_data_ != NULL) 
        {
            m_len_ = s.length();
            m_data_[m_len_] = '\0';
            if (temp != NULL)
            {
                free(temp);
            }
        }

        return *this;
    }

    NcString& operator=(const uint8_t *s)
    {
        if (m_data_ == s)
        {
            return *this;
        }

        uint8_t *temp = m_data_;
        m_len_ = strlen((const char*)s);
        m_data_ = nc_strndup(s, m_len_ + 1);
        if (m_data_ != NULL) 
        {
            m_data_[m_len_] = '\0';
            if (temp != NULL)
            {
                free(temp);
            }
        }
        else
        {
            m_len_ = 0;
        }
        
        return *this;
    }

    bool operator==(const NcString &s)
    {
        if (m_len_ <= 0)
        {
            return false;
        }

        int status = strcmp((char*)m_data_, (const char*)s.c_str());
        return status == 0 ? true : false;
    }

    bool operator==(const uint8_t *s)
    {
        if (m_len_ <= 0 || s == NULL)
        {
            return false;
        }

        int status = strcmp((char*)m_data_, (const char*)s);
        return status == 0 ? true : false;
    }

    void setText(const uint8_t *text, uint32_t len)
    {
        m_len_ = len - 1;
        m_data_ = (uint8_t*)(text);
    }

    void setRaw(const uint8_t *raw)
    {
        m_len_ = (uint32_t)(nc_strlen(raw));
        m_data_ = (uint8_t*)(raw);
    }

    const uint8_t* c_str() const
    {
        return m_data_;
    }

    uint32_t length() const
    {
        return m_len_;
    }

    bool empty()
    {
        return m_len_ == 0 ? true : false;
    }

    int compare(const NcString &s)
    {
        if (m_len_ != s.length()) 
        {
            return m_len_ > s.length() ? 1 : -1;
        }

        return nc_strncmp(m_data_, s.c_str(), m_len_);
    }

    static inline uint8_t* _nc_strchr(uint8_t *p, uint8_t *last, uint8_t c)
    {
        while (*p != '\0' && p < last && *p++ != c);
        return p >= last ? NULL : p - 1;
    }

    static inline uint8_t* _nc_strrchr(uint8_t *p, uint8_t *start, uint8_t c)
    {
        while (*p != '\0' && p >= start && *p-- != c);
        return p < start ? NULL : p + 1;
    }
    
private:
    uint32_t m_len_;   /* string length */
    uint8_t  *m_data_; /* string data */
};

#endif