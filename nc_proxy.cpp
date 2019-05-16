#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <nc_util.h>
#include <nc_proxy.h>

#include <nc_client.h>
#include <nc_log.h>
#include <nc_server.h>

void NcProxyConn::ref(void *owner)
{
    FUNCTION_INTO(NcProxyConn);

    m_owner_ = owner;
    NcServerPool *pool = (NcServerPool*)m_owner_;
    m_family_ = pool->info.family;
    m_addrlen_ = pool->info.addrlen;
    m_addr_ = (struct sockaddr*)(&(pool->info.addr));

    pool->p_conn = this;

    LOG_DEBUG("ref conn %p owner %p into pool %" PRIu32 "", this, pool, pool->idx);
}

void NcProxyConn::unref()
{
    FUNCTION_INTO(NcProxyConn);

    NcServerPool *pool = (NcServerPool*)m_owner_;
    m_owner_ = NULL;
    pool->p_conn = NULL;

    LOG_DEBUG("unref conn %p owner %p from pool %" PRIu32 "", this, pool, pool->idx);
}

bool NcProxyConn::active()
{
    FUNCTION_INTO(NcProxyConn);

    return true;
}

void NcProxyConn::close()
{
    FUNCTION_INTO(NcProxyConn);

    NcContext *ctx = (NcContext*)getContext();

    unref();

    if (m_sd_ < 0)
    {
        (ctx->p_pool).free(this);
        return ;
    }

    rstatus_t status = ::close(m_sd_);
    if (status < 0)
    {
        LOG_ERROR("close c %d failed, ignored: %s", m_sd_, strerror(errno));
    }
    
    m_sd_ = -1;
    (ctx->p_pool).free(this);
}

rstatus_t NcProxyConn::recvMsg()
{
    FUNCTION_INTO(NcProxyConn);
    
    m_recv_ready_ = 1;
    do 
    {
        rstatus_t status = accept();
        if (status != NC_OK) 
        {
            return status;
        }
    } while (m_recv_ready_);

    LOG_DEBUG("m_recv_ready_ : %d", m_recv_ready_);

    return NC_OK;
}

rstatus_t NcProxyConn::sendMsg()
{
    FUNCTION_INTO(NcProxyConn);

    return NC_OK;
}

void* NcProxyConn::getContext()
{
    NcServerPool *pool = (NcServerPool*)m_owner_;
    if (pool == NULL)
    {
        return NULL;
    }

    NcContext *ctx = pool->ctx;
    return ctx;
}

rstatus_t NcProxyConn::reuse()
{
    FUNCTION_INTO(NcProxyConn);

    rstatus_t status;
    struct sockaddr_un *un;

    switch (m_family_) 
    {
    case AF_INET:
    case AF_INET6:
        status = NcUtil::nc_set_reuseaddr(m_sd_);
        break;

    case AF_UNIX:
        un = (struct sockaddr_un*)m_addr_;
        ::unlink(un->sun_path);
        status = NC_OK;
        break;

    default:
        status = NC_ERROR;
        break;
    }

    return status;
}

rstatus_t NcProxyConn::listen()
{
    FUNCTION_INTO(NcProxyConn);

    NcServerPool *pool = (NcServerPool *)m_owner_;
    NcContext *ctx = pool->ctx;

    LOG_DEBUG("m_family_ : %d", m_family_);
    m_sd_ = ::socket(m_family_, SOCK_STREAM, 0);
    if (m_sd_ < 0) 
    {
        LOG_ERROR("socket failed: %s", strerror(errno));
        return NC_ERROR;
    }

    // 重置连接
    rstatus_t status = reuse();
    if (status < 0) 
    {
        LOG_ERROR("reuse of addr '%.*s' for listening on p %d failed: %s",
            pool->addrstr.length(), pool->addrstr.c_str(), 
            m_sd_, strerror(errno));
        return NC_ERROR;
    }

    status = ::bind(m_sd_, m_addr_, m_addrlen_);
    if (status < 0) 
    {
        LOG_ERROR("bind on p %d to addr '%.*s' failed: %s", m_sd_,
            pool->addrstr.length(), pool->addrstr.c_str(), strerror(errno));
        return NC_ERROR;
    }

    if (m_family_ == AF_UNIX && pool->perm) 
    {
        struct sockaddr_un *un = (struct sockaddr_un *)m_addr_;
        status = ::chmod(un->sun_path, pool->perm);
        if (status < 0) 
        {
            LOG_ERROR("chmod on p %d on addr '%.*s' failed: %s", m_sd_,
                pool->addrstr.length(), pool->addrstr.c_str(), strerror(errno));
            return NC_ERROR;
        }
    }

    status = ::listen(m_sd_, pool->backlog);

    LOG_DEBUG("backlog : %d, status : %d, m_sd_ : %d", pool->backlog, status, m_sd_);

    if (status < 0) 
    {
        LOG_ERROR("listen on p %d on addr '%.*s' failed: %s", m_sd_, 
            pool->addrstr.length(), pool->addrstr.c_str(), strerror(errno));
        return NC_ERROR;
    }

    status = NcUtil::nc_set_nonblocking(m_sd_);
    if (status < 0) 
    {
        LOG_ERROR("set nonblock on p %d on addr '%.*s' failed: %s", m_sd_,
            pool->addrstr.length(), pool->addrstr.c_str(), strerror(errno));
        return NC_ERROR;
    }

    LOG_DEBUG("ctx : %p, evb : %p", ctx, &(ctx->getEvb()));

    status = ctx->getEvb().addConn(this);
    if (status < 0) 
    {
        FUNCTION_OUT(NcProxyConn);
        LOG_ERROR("event add conn p %d on addr '%.*s' failed: %s",
            m_sd_, pool->addrstr.length(), pool->addrstr.c_str(),
            strerror(errno));
        return NC_ERROR;
    }

    status = (ctx->getEvb()).delOutput(this);
    if (status < 0) 
    {
        FUNCTION_OUT(NcProxyConn);
        LOG_ERROR("event del out p %d on addr '%.*s' failed: %s",
            m_sd_, pool->addrstr.length(), pool->addrstr.c_str(),
            strerror(errno));
        return NC_ERROR;
    }

    FUNCTION_OUT(NcProxyConn);

    return NC_OK;
}

rstatus_t NcProxyConn::accept()
{
    FUNCTION_INTO(NcProxyConn);
    
    NcServerPool *pool = (NcServerPool *)m_owner_;
    NcContext *ctx = (NcContext*)getContext();

    int sd = -1;

    for (;;) 
    {
        sd = ::accept(m_sd_, NULL, NULL);
        if (sd < 0) 
        {
            if (errno == EINTR) 
            {
                LOG_DEBUG("accept on p %d not ready - eintr", m_sd_);
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED) 
            {
                LOG_DEBUG("accept on p %d not ready - eagain", m_sd_);
                m_recv_ready_ = 0;

                return NC_OK;
            }

            if (errno == EMFILE || errno == ENFILE) 
            {
                LOG_DEBUG("accept on p %d with max fds %" PRIu32 " "
                    "used connections %" PRIu32 " max client connections %" PRIu32 " "
                    "curr client connections %" PRIu32 " failed: %s",
                    m_sd_, ctx->max_nfd, NcUtil::ncCurConn(),
                    ctx->max_ncconn, NcUtil::ncCurCConn(), strerror(errno));

                m_recv_ready_ = 0;

                return NC_OK;
            }

            LOG_ERROR("accept on p %d failed: %s", m_sd_, strerror(errno));

            return NC_ERROR;
        }
        
        break;
    }

    rstatus_t status;
    if (NcUtil::ncCurCConn() >= ctx->max_ncconn) 
    {
        LOG_DEBUG("client connections %" PRIu32 " exceed limit %" PRIu32,
                  NcUtil::ncCurCConn(), ctx->max_ncconn);
        status = ::close(sd);
        if (status < 0) 
        {
            LOG_ERROR("close c %d failed, ignored: %s", sd, strerror(errno));
        }

        return NC_OK;
    }

    NcConn *conn = (NcConn*)(ctx->c_pool).alloc<NcClientConn>();
    if (conn == NULL) 
    {
        LOG_ERROR("get conn for c %d from p %d failed: %s", sd, m_sd_, strerror(errno));
        status = ::close(sd);
        if (status < 0) 
        {
            LOG_ERROR("close c %d failed, ignored: %s", sd, strerror(errno));
        }

        return NC_ENOMEM;
    }

    conn->setSd(sd);
    conn->ref(m_owner_);
    status = NcUtil::nc_set_nonblocking(conn->getSd());
    if (status < 0) 
    {
        LOG_ERROR("set nonblock on c %d from p %d failed: %s", 
            conn->getSd(), m_sd_, strerror(errno));
        conn->close();
        return status;
    }

    if (pool->tcpkeepalive) 
    {
        status = NcUtil::nc_set_tcpkeepalive(conn->getSd());
        if (status < 0) 
        {
            LOG_WARN("set tcpkeepalive on c %d from p %d failed, ignored: %s",
                conn->getSd(), m_sd_, strerror(errno));
        }
    }

    if (m_family_ == AF_INET || m_family_ == AF_INET6) 
    {
        status = NcUtil::nc_set_tcpnodelay(conn->getSd());
        if (status < 0) 
        {
            LOG_WARN("set tcpnodelay on c %d from p %d failed, ignored: %s",
                conn->getSd(), m_sd_, strerror(errno));
        }
    }

    status = (ctx->getEvb()).addConn(conn);
    if (status < 0) 
    {
        LOG_ERROR("event add conn from p %d failed: %s", m_sd_, strerror(errno));
        conn->close();
        return status;
    }

    LOG_DEBUG("accepted c %d on p %d", conn->getSd(), m_sd_);
    
    return NC_OK;
}