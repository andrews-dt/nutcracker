#include <nc_core.h>
#include <nc_connection.h>
#include <nc_proxy.h>
#include <nc_message.h>
#include <nc_server.h>
#include <nc_client.h>
#include <nc_proxy.h>

rstatus_t NcContext::createProxyConn()
{
    NcProxyConn *conn = (NcProxyConn*)p_pool.alloc<NcProxyConn>();
    conn->ref(server_pool);
    rstatus_t status = conn->listen();

    if (status != NC_OK) 
    {
        conn->close();
        nc_delete(conn);
        return status;
    }

    NcServerPool *pool = (NcServerPool*)server_pool;
    ASSERT(pool != NULL);
    LOG_DEBUG("conn %d listening on '%s' and '%s'", conn->m_sd_, 
              pool->addrstr.c_str(), pool->name.c_str());

    return NC_OK;
}

rstatus_t NcContext::createServerConn()
{
    NcServerPool *pool = (NcServerPool*)server_pool;
    ASSERT(pool != NULL);

    for (int i = 0; i < pool->server.size(); i++)
    {
        NcServerConn *conn = (NcServerConn*)p_pool.alloc<NcServerConn>();
        conn->ref((pool->server)[i]);

        rstatus_t status = conn->connect();
        if (status != NC_OK) 
        {
            conn->close();
            nc_delete(conn);
            return status;
        }
    }
    
    return NC_OK;
}

rstatus_t NcContext::calcConnections()
{
    int status;
    struct rlimit limit;

    status = ::getrlimit(RLIMIT_NOFILE, &limit);
    if (status < 0) 
    {
        LOG_ERROR("getrlimit failed: %s", strerror(errno));
        return NC_ERROR;
    }

    max_nfd = (uint32_t)limit.rlim_cur;
    max_ncconn = max_nfd - max_nsconn - RESERVED_FDS;
    LOG_DEBUG("max fds %" PRIu32 " max client conns %" PRIu32 " "
        "max server conns %" PRIu32 "", 
        max_nfd, max_ncconn, max_nsconn);
        
    return NC_OK;
}

NcContext* NcInstance::createContext(NcConfPool *_pool)
{
    FUNCTION_INTO(NcInstance);

    NcContext *_ctx = new NcContext();

    _ctx->id = NcUtil::uniqNextId();
    _ctx->max_timeout = stats_interval;
    _ctx->timeout = _ctx->max_timeout;
    _ctx->max_nfd = 0;
    _ctx->max_ncconn = 0;
    _ctx->max_nsconn = 0;
    _ctx->server_pool = new NcServerPool(_ctx);
    _ctx->instance = this;
    NcUtil::ncMbufChunkSize(mbuf_chunk_size);

    // 通过配置文件初始化，TODO：默认使用第一个配置
    ((NcServerPool*)(_ctx->server_pool))->setConf(_pool);

    rstatus_t status = _ctx->calcConnections();
    if (status != NC_OK)
    {
        LOG_ERROR("calcConnections error!!!");
        nc_delete(_ctx);
        return NULL;
    }

    // 初始化server链接
    status = _ctx->createServerConn();
    if (status != NC_OK)
    {
        LOG_ERROR("createServerConn error!!!");
        nc_delete(_ctx);
        return NULL;
    }

    // 初始化proxy的链接
    status = _ctx->createProxyConn();
    if (status != NC_OK)
    {
        LOG_ERROR("createProxyConn error!!!");
        nc_delete(_ctx);
        return NULL;
    }

    return _ctx;
}

rstatus_t NcInstance::loop()
{
    FUNCTION_INTO(NcInstance);

    // TODO : 
    timeout = -1;
    int nsd = evb.wait(timeout);
    LOG_DEBUG("nsd : %d", nsd);
    if (nsd < 0) 
    {
        return nsd;
    }

    // 提取超时的节点，并处理
    for (;;) 
    {
        rbnode *node = tmo_rbe.min();
        LOG_DEBUG("node : %p", node);
        NcMsgBase *msg = (NcMsgBase*)node;
        if (msg == NULL) 
        {
            // TODO :
            // timeout = ctx->max_timeout;
            break;
        }

        if (msg->m_error_ || msg->m_done_) 
        {
            tmo_rbe.remove((rbnode *)msg);
            continue;
        }

        NcConn *conn = (NcConn*)(msg->data);
        int64_t then = msg->key;

        int64_t now = NcUtil::ncMsecNow();
        if (now < then) 
        {
            int delta = (int)(then - now);
            // TODO :
            // timeout = MIN(delta, ctx->max_timeout);
            break;
        }

        LOG_DEBUG("req %" PRIu64 " on s %d timedout", msg->m_id_, conn->getSd());

        tmo_rbe.remove((rbnode*)msg);
        conn->setError(ETIMEDOUT);
        conn->processClose();
    }

    FUNCTION_OUT(NcInstance);

    return NC_OK;
}