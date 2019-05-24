#ifndef _NC_SERVER_H_
#define _NC_SERVER_H_

#include <nc_util.h>
#include <nc_string.h>
#include <nc_queue.h>
#include <nc_conf.h>
#include <nc_log.h>
#include <nc_connection.h>

/*
 * server_pool is a collection of servers and their continuum. Each
 * server_pool is the owner of a single proxy connection and one or
 * more client connections. server_pool itself is owned by the current
 * context.
 *
 * Each server is the owner of one or more server connections. server
 * itself is owned by the server_pool.
 *
 *  +-------------+
 *  |             |<---------------------+
 *  |             |<------------+        |
 *  |             |     +-------+--+-----+----+--------------+
 *  |   pool 0    |+--->|          |          |              |
 *  |             |     | server 0 | server 1 | ...     ...  |
 *  |             |     |          |          |              |--+
 *  |             |     +----------+----------+--------------+  |
 *  +-------------+                                             //
 *  |             |
 *  |             |
 *  |             |
 *  |   pool 1    |
 *  |             |
 *  |             |
 *  |             |
 *  +-------------+
 *  |             |
 *  |             |
 *  .             .
 *  .    ...      .
 *  .             .
 *  |             |
 *  |             |
 *  +-------------+
 *            |
 *            |
 *            //
 */

class NcServerPool;
class NcServerConn;
class NcServer;

class NcContinuum 
{
public:
    uint32_t index;  /* server index */
    uint32_t value;  /* hash value */
};

class NcServerPool
{
public:
    NcServerPool(NcContext *_ctx)
    {
        ctx = _ctx;
        ncontinuum = 1;
    }

    // 通过conf获取配置信息
    void setConf(NcConfPool *_pool);

    inline void setContext(NcContext *_ctx)
    {
        ctx = _ctx;
    }

    rstatus_t update();

    uint32_t index(uint8_t *key, uint32_t keylen);

    NcConn* getConn(uint8_t *key, uint32_t keylen);

public:
    uint32_t           idx;                  /* pool index */
    NcContext          *ctx;                 /* owner context */

    NcConnBase         *p_conn;             /* proxy connection (listener) */
    uint32_t           nc_conn_q;           /* # client connection */
    NcQueue<NcConnBase*>   c_conn_q;        /* client connection q */

    std::vector<NcServer*>  server;          /* server[] */
    uint32_t           nlive_server;         /* # live server */
    int64_t            next_rebuild;         /* next distribution rebuild time in usec */

    NcString           name;                 /* pool name (ref in conf_pool) */
    NcString           addrstr;              /* pool address - hostname:port (ref in conf_pool) */
    uint16_t           port;                 /* port */
    struct sockinfo    info;                 /* listen socket info */
    mode_t             perm;                 /* socket permission */
    int                dist_type;            /* distribution type (dist_type_t) */
    int                key_hash_type;        /* key hash type (hash_type_t) */
    NcString           hash_tag;             /* key hash tag (ref in conf_pool) */
    int                timeout;              /* timeout in msec */
    int                backlog;              /* listen backlog */
    int                redis_db;             /* redis database to connect to */
    uint32_t           client_connections;   /* maximum # client connection */
    uint32_t           server_connections;   /* maximum # server connection */
    int64_t            server_retry_timeout; /* server retry timeout in usec */
    uint32_t           server_failure_limit; /* server failure limit */
    unsigned           tcpkeepalive;         /* tcpkeepalive? */

    unsigned           auto_eject_hosts;     /* auto_eject_hosts? */
    unsigned           preconnect;           /* preconnect? */

    uint32_t           ncontinuum;           /* # continuum points */
    uint32_t           nserver_continuum;    /* # servers - live and dead on continuum (const) */
    NcContinuum        *continuum;
};

class NcServer 
{
    friend class NcServerConn;
    friend class NcServerPool;

public:
    NcServer(NcServerPool *_pool) : m_server_pool_(_pool)
    { }

    NcConn* getConn();

    inline NcContext* getContext()
    {
        ASSERT(m_server_pool_ != NULL);
        return m_server_pool_->ctx;
    }

    inline NcServerPool* getServerPool()
    {
        return m_server_pool_;
    }

    // 通过conf获取配置信息
    inline void setConf(NcConfServer *server)
    {
        FUNCTION_INTO(NcServer);

        m_name_ = server->name;
        m_addrstr_ = server->name;
        m_pname_ = server->pname;
        m_port_ = (uint16_t)server->port;
        nc_memcpy(&m_info_, &server->info, sizeof(server->info));
        m_weight_ = server->weight;

        LOG_DEBUG("info.family : %d, addstr : %s, port : %d, weight: %d", 
            m_info_.family, m_addrstr_.c_str(), m_port_, m_weight_);
    }

    inline int64_t getNextRetry()
    {
        return m_next_retry_;
    }

    inline void setNextRetry(int64_t next_retry)
    {
        m_next_retry_ = next_retry;
    }

    inline uint32_t getWeight()
    {
        return m_weight_;
    }

    inline void setWeight(uint32_t weight)
    {
        m_weight_ = weight;
    }

private:
    NcServerPool    *m_server_pool_;
    uint32_t        m_idx_;           /* server index */

    NcString        m_pname_;         /* hostname:port:weight (ref in conf_server) */
    NcString        m_name_;          /* hostname:port or [name] (ref in conf_server) */
    NcString        m_addrstr_;       /* hostname (ref in conf_server) */
    uint16_t        m_port_;          /* port */
    uint32_t        m_weight_;        /* weight */

    struct sockinfo m_info_;          /* server socket info */
    NcQueue<NcConnBase*> m_conn_queue_;

    uint32_t        m_ns_conn_q_;     /* # server connection */
    int64_t         m_next_retry_;    /* next retry time in usec */
    uint32_t        m_failure_count_; /* # consecutive failures */
};

class NcServerConn : public NcConn
{
public:
    virtual void ref(void *owner = NULL);
    virtual void unref();
    virtual bool active();
    virtual void close();

    virtual void enqueueInput(NcMsgBase *msg)
    { 
        m_imsg_q_.push(msg);
    }

    virtual void dequeueInput(NcMsgBase *msg)
    { 
        m_imsg_q_.remove(msg);
    }

    virtual void enqueueOutput(NcMsgBase *msg)
    {
        m_omsg_q_.push(msg);
    }

    virtual void dequeueOutput(NcMsgBase *msg)
    {
        m_omsg_q_.remove(msg);
    }

    rstatus_t connect(); 

    virtual NcMsgBase* recvNext(bool alloc);

    virtual void recvDone(NcMsgBase *cmsg, NcMsgBase *rmsg);

    virtual NcMsgBase* sendNext();

    virtual void sendDone(NcMsgBase *msg);

    virtual void* getContext();
};

#endif