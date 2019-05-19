#ifndef _NC_CORE_H_
#define _NC_CORE_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_DEBUG_LOG
# define NC_DEBUG_LOG 1
#endif

#ifdef HAVE_ASSERT_PANIC
# define NC_ASSERT_PANIC 1
#endif

#ifdef HAVE_ASSERT_LOG
# define NC_ASSERT_LOG 1
#endif

#ifdef HAVE_STATS
# define NC_STATS 1
#else
# define NC_STATS 0
#endif

#ifdef HAVE_EPOLL
# define NC_HAVE_EPOLL 1
#elif HAVE_KQUEUE
# define NC_HAVE_KQUEUE 1
#elif HAVE_EVENT_PORTS
# define NC_HAVE_EVENT_PORTS 1
#else
# error missing scalable I/O event notification mechanism
#endif

#ifdef HAVE_LITTLE_ENDIAN
# define NC_LITTLE_ENDIAN 1
#endif

#ifdef HAVE_BACKTRACE
# define NC_HAVE_BACKTRACE 1
#endif

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
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <queue>
#include <nc_event.h>
#include <nc_log.h>
#include <nc_string.h>
#include <nc_rbtree.h>
#include <nc_util.h>
#include <nc_mbuf.h>
#include <nc_queue.h>
#include <nc_signal.h>
#include <nc_conf.h>

class NcInstance;
class NcContext;
class NcMsgBase;

typedef enum
{
    kPROTOCOL_HTTP,
    kPROTOCOL_REDIS,
    kPROTOCOL_MEMCACHED,
    kPROTOCOL_MYSQL,
} NcProtocolType;

template<class T>
class NcObjectPool
{
public:
    typedef std::queue<T> NcObjectQueue;

    NcObjectPool() : m_free_(0)
    { }

    ~NcObjectPool()
    {
        T o = NULL;
        while (!m_queue_.empty())
        {
            o = m_queue_.front();
            m_queue_.pop();
            if (o != NULL)
            {
                delete o;
            }
        }
    }

    template<class RT>
    inline T alloc(void *args = NULL)
    {
        T o = NULL;
        if (!m_queue_.empty())
        {
            o = m_queue_.front();
            m_queue_.pop();
        }
        else
        {
            o = new RT();
        }

        m_current_ = o;
        return o;
    }

    inline void free(T o)
    {
        if (o == NULL)
        {
            return ;
        }
        
        m_queue_.push(o);
        m_free_++;
    }

    inline T current()
    {
        return m_current_;
    }

private:
    NcObjectQueue   m_queue_;
    uint32_t        m_free_;
    T               m_current_;
};

class NcInstance 
{
public:
    rstatus_t calcConnections();

    rstatus_t loop();

    NcContext* createContext(NcConfPool *_pool);

public:
    std::vector<NcContext*>     ctx;            /* active context */
    int             log_level;                   /* log level */
    char            *log_filename;               /* log filename */
    char            *conf_filename;              /* configuration filename */
    
    // 监控的数据
    uint16_t        stats_port;                  /* stats monitoring port */
    int             stats_interval;              /* stats aggregation interval */
    char            *stats_addr;                 /* stats monitoring addr */

    char            hostname[NC_MAXHOSTNAMELEN]; /* hostname */
    size_t          mbuf_chunk_size;             /* mbuf chunk size */
    pid_t           pid;                         /* process id */
    char            *pid_filename;               /* pid filename */
    unsigned        pidfile;                     /* pid file created? */

    NcSignal        *signal;
    NcConf          conf;                       // 配置文件

    NcEventBase     evb;                        /* event base */
    NcRbTree        tmo_rbe;                    // 红黑树

    int             timeout;                    // 超时的timeout
};

class NcContext 
{
public:
    NcContext() : server_pool(NULL), instance(NULL)
    { }

    rstatus_t createProxyConn();

    rstatus_t createServerConn();

    rstatus_t calcConnections();

    inline void setInstance(NcInstance *_instance)
    {
        instance = _instance;
    }

    inline NcInstance* getInstance()
    {
        return instance;
    }

    inline NcEventBase& getEvb()
    {
        return instance->evb;
    }

    inline NcRbTree& getRBTree()
    {
        return instance->tmo_rbe;
    }

public:
    NcObjectPool<NcMbuf*>           mbuf_pool;
    NcObjectPool<NcConnBase*>       c_pool, s_pool, p_pool;
    NcObjectPool<NcMsgBase*>        msg_pool;

    uint32_t        id;             /* unique context id */
    int             max_timeout;    /* max timeout in msec */
    int             timeout;        /* timeout in msec */

    uint32_t        max_nfd;        /* max # files */
    uint32_t        max_ncconn;     /* max # client connections */
    uint32_t        max_nsconn;     /* max # server connections */

    void            *server_pool;
    NcInstance      *instance;
    NcProtocolType  protocol_type; // 协议类型
};

// 继承rbtree的节点信息
class NcMsgBase : public rbnode
{
public:
    void reset()
    {
        rbnode::reset();

        m_id_ = NcUtil::uniqNextId();
        m_peer_ = NULL;
        m_mlen_ = 0; 
        m_start_ts_ = NcUtil::ncUsecNow();
        pos = NULL;

        m_err_ = 0;
        m_type_ = kPROTOCOL_HTTP;
    }

    inline void setProtocolType(NcProtocolType type)
    {
        m_type_ = type;
    }

    inline void setPos(uint8_t *_pos)
    {
        pos = _pos;
    }

    inline void setLength(uint32_t len)
    {
        m_mlen_ = len;
    }

    inline bool empty()
    {
        return m_mlen_ == 0 ? true : false;
    }

public:
    uint64_t        m_id_;              /* message id */
    NcMsgBase       *m_peer_;           /* message peer */
    uint32_t        m_mlen_;            /* message length */  
    int64_t         m_start_ts_;        /* request start timestamp in usec */
    uint8_t         *pos;
    int             state; 

    err_t           m_err_;             /* errno on error? */
    unsigned        m_error_;           /* error? */
    unsigned        m_ferror_;          /* one or more fragments are in error? */
    unsigned        m_request_;         /* request? or response? */
    unsigned        m_quit_;            /* quit request? */
    unsigned        m_noreply_;         /* noreply? */
    unsigned        m_noforward_;       /* not need forward (example: ping) */
    unsigned        m_done_;            /* done? */
    unsigned        m_fdone_;           /* all fragments are done? */
    unsigned        m_swallow_;         /* swallow response? */
    NcProtocolType  m_type_;            // 类型
};

#endif
