#ifndef _NC_EVENT_H_
#define _NC_EVENT_H_

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nc_log.h>

#define EVENT_SIZE  1024

#define EVENT_READ  0x000001
#define EVENT_WRITE 0x000010
#define EVENT_ERR   0x000100

#define NC_EVENT_OK        0
#define NC_EVENT_ERROR    -1
#define NC_EVENT_EAGAIN   -2
#define NC_EVENT_ENOMEM   -3

#ifdef NC_HAVE_KQUEUE

struct _event 
{
public:
    int           kq;          /* kernel event queue descriptor */

    struct kevent *change;     /* change[] - events we want to monitor */
    int           nchange;     /* # change */

    struct kevent *event;      /* event[] - events that were triggered */
    int           nevent;      /* # event */
    int           nreturned;   /* # event placed in event[] */
    int           nprocessed;  /* # event processed from event[] */
};

#elif NC_HAVE_EPOLL

struct _event 
{
    int                ep;      /* epoll descriptor */

    struct epoll_event *event;  /* event[] - events that were triggered */
    int                nevent;  /* # event */
};

#else
# error missing scalable I/O event notification mechanism
#endif

typedef enum
{
    NC_UNKOWN = 0x00,
    NC_PROXY,   // proxy connection
    NC_CLIENT,  // client connection
    NC_SERVER   // server connection
} NcConnType;

class NcConnBase
{
public:
    virtual int callback(uint32_t events) = 0;

    ssize_t recv(void *buf, size_t size)
    {
        ssize_t n;

        for (;;) 
        {
            n = ::read(m_sd_, buf, size);

            LOG_DEBUG("recv on sd %d %zd of %zu", m_sd_, n, size);

            if (n > 0) 
            {
                if (n < (ssize_t) size) 
                {
                    m_recv_ready_ = 0;
                }
                m_recv_bytes_ += (size_t)n;
                return n;
            }

            if (n == 0) 
            {
                m_recv_ready_ = 0;
                m_eof_ = 1;
                LOG_DEBUG("recv on sd %d eof rb %zu sb %zu", m_sd_,
                        m_recv_bytes_, m_send_bytes_);
                return n;
            }

            if (errno == EINTR) 
            {
                LOG_DEBUG("recv on sd %d not ready - eintr", m_sd_);
                continue;
            } 
            else if (errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                m_recv_ready_ = 0;
                LOG_DEBUG("recv on sd %d not ready - eagain", m_sd_);
                return NC_EVENT_EAGAIN;
            } 
            else 
            {
                m_recv_ready_ = 0;
                m_err_ = errno;
                LOG_ERROR("recv on sd %d failed: %s", m_sd_, strerror(errno));
                return NC_EVENT_ERROR;
            }
        }

        return NC_EVENT_ERROR;
    }
    
    ssize_t send(void *buf, size_t size)
    {
        ssize_t n;

        for (;;) 
        {
            n = ::write(m_sd_, buf, size);

            LOG_DEBUG("sendv on sd %d %zd of %zu", m_sd_, n, size);

            if (n > 0) 
            {
                if (n < (ssize_t)size)
                {
                    m_send_ready_ = 0;
                }
                m_send_bytes_ += (size_t)n;
                return n;
            }

            if (n == 0) 
            {
                LOG_WARN("sendv on sd %d returned zero", m_sd_);
                m_send_ready_ = 0;
                return 0;
            }

            if (errno == EINTR) 
            {
                LOG_DEBUG("sendv on sd %d not ready - eintr", m_sd_);
                continue;
            } 
            else if (errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                m_send_ready_ = 0;
                LOG_DEBUG("sendv on sd %d not ready - eagain", m_sd_);
                return NC_EVENT_EAGAIN;
            } 
            else 
            {
                m_send_ready_ = 0;
                m_err_ = errno;
                LOG_ERROR("sendv on sd %d failed: %s", m_sd_, strerror(errno));
                return NC_EVENT_ERROR;
            }
        }
        
        return NC_EVENT_ERROR;
    }
    
    // 聚集写
    ssize_t sendv(void *buf, size_t size)
    {
        ssize_t n;
        struct iovec *_buf = (struct iovec*)buf;

        for (;;) 
        {
            n = ::writev(m_sd_, _buf, size);

            LOG_DEBUG("sendv on sd %d %zd of %zu", m_sd_, n, size);

            if (n > 0) 
            {
                if (n < (ssize_t)size)
                {
                    m_send_ready_ = 0;
                }
                m_send_bytes_ += (size_t)n;
                return n;
            }

            if (n == 0) 
            {
                LOG_WARN("sendv on sd %d returned zero", m_sd_);
                m_send_ready_ = 0;
                return 0;
            }

            if (errno == EINTR) 
            {
                LOG_DEBUG("sendv on sd %d not ready - eintr", m_sd_);
                continue;
            } 
            else if (errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                m_send_ready_ = 0;
                LOG_DEBUG("sendv on sd %d not ready - eagain", m_sd_);
                return NC_EVENT_EAGAIN;
            } 
            else 
            {
                m_send_ready_ = 0;
                m_err_ = errno;
                LOG_ERROR("sendv on sd %d failed: %s", m_sd_, strerror(errno));
                return NC_EVENT_ERROR;
            }
        }
        
        return NC_EVENT_ERROR;
    }

    inline void reset()
    {
        m_sd_ = -1;
        m_events_ = 0;
        m_err_ = 0;
        m_family_ = AF_INET;
        m_addrlen_ = sizeof(struct sockaddr);
        m_addr_ = NULL;

        m_recv_active_ = 0;
        m_recv_ready_ = 0;
        m_send_active_ = 0;
        m_send_ready_ = 0;

        m_eof_ = 0;
        m_done_ = 0;

        m_send_bytes_ = 0;
        m_recv_bytes_ = 0;
    }

    inline void setSd(int sd)
    {
        m_sd_ = sd;
    }

    inline int getSd()
    {
        return m_sd_;
    }

    inline void setNcConnType(NcConnType _type)
    {
        m_conn_type_ = _type;
    }

    inline void setData(void *data)
    {
        m_data_ = data;
    }

    inline void setError(int err)
    {
        m_err_ = err;
    }

    inline int getError()
    {
        return m_err_;
    }

public:
    int         m_sd_;            /* socket descriptor */
    uint32_t    m_events_;        /* connection io events */
    int         m_err_;           /* connection errno */
    int         m_family_;          /* socket address family */
    socklen_t   m_addrlen_;         /* socket length */
    struct sockaddr     *m_addr_;           /* socket address (ref in server or server_pool) */
    
    unsigned    m_recv_active_;   /* recv active? */
    unsigned    m_recv_ready_;    /* recv ready? */
    unsigned    m_send_active_;   /* send active? */
    unsigned    m_send_ready_;    /* send ready? */

    size_t      m_recv_bytes_;      /* received (read) bytes */
    size_t      m_send_bytes_;      /* sent (written) bytes */

    unsigned    m_eof_;           /* eof? aka passive close? */
    unsigned    m_done_;          /* done? aka close? */

    NcConnType  m_conn_type_;     // 连接类型
    void*       m_data_;
};

class NcEventBase
{
public:
    NcEventBase(int size = EVENT_SIZE);

    ~NcEventBase();

    int addInput(NcConnBase *c);

    int delInput(NcConnBase *c);

    int addOutput(NcConnBase *c);

    int delOutput(NcConnBase *c);

    int addConn(NcConnBase *c, uint32_t events = EVENT_READ | EVENT_WRITE);

    int delConn(NcConnBase *c);

    int wait(int timeout);

    void loop(void *arg);

private:
    struct _event *m_event_;
};

#endif /* _NC_EVENT_H */
