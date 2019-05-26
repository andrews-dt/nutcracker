#include <nc_server.h>
#include <nc_message.h>
#include <nc_hashkit.h>

NcConn* NcServer::getConn()
{
    if (m_server_pool_ == NULL)
    {
        LOG_ERROR("m_server_pool_ is NULL");
        return NULL;
    }

    NcContext *ctx = getContext();
    if (ctx == NULL)
    {
        LOG_ERROR("ctx is NULL");
        return NULL;
    }

    if (m_ns_conn_q_ < m_server_pool_->server_connections) 
    {
        return (NcConn*)(m_server_pool_->ctx->s_pool).alloc<NcServerConn>();
    }

    NcConnBase *conn = m_conn_queue_.front();
    m_conn_queue_.remove(conn);
    m_conn_queue_.push(conn);

    return (NcConn*)conn;
}

void NcServer::failure()
{
    NcServerPool *pool = m_server_pool_;
    ASSERT(pool != NULL);

    int64_t now, next;
    rstatus_t status;

    if (!pool->auto_eject_hosts) 
    {
        return;
    }

    m_failure_count_++;

    LOG_DEBUG("server '%.*s' failure count %" PRIu32,
        m_pname_.length(), m_pname_.c_str(), m_failure_count_);

    if (m_failure_count_ < pool->server_failure_limit) 
    {
        LOG_DEBUG("failure count(%d) > failure limit(%d)", 
            m_failure_count_, pool->server_failure_limit);
        return;
    }

    now = NcUtil::ncUsecNow();
    if (now < 0) 
    {
        LOG_DEBUG("now %d < 0", now);
        return;
    }

    next = now + pool->server_retry_timeout;

    LOG_DEBUG("update pool %" PRIu32 " '%.*s' to delete server '%.*s' "
        "for next %" PRIu32 " secs", pool->idx, pool->name.length(),
        pool->name.c_str(), m_pname_.length(), m_pname_.c_str(),
        pool->server_retry_timeout / 1000 / 1000);
    m_failure_count_ = 0;
    m_next_retry_ = next;

    status = pool->update();
    if (status != NC_OK) 
    {
        LOG_ERROR("updating pool %"PRIu32" '%.*s' failed: %s", pool->idx,
            pool->name.length(), pool->name.c_str(), strerror(errno));
    }
}

rstatus_t NcServer::preconnect()
{
    NcConn *conn = getConn();
    if (conn == NULL)
    {
        return NC_ENOMEM;
    }

    rstatus_t status = ((NcServerConn*)conn)->connect();
    if (status != NC_OK)
    {
        LOG_WARN("connect to server '%.*s' failed, ignored: %s",
            m_pname_.length(), m_pname_.c_str(), strerror(errno));
        conn->close();
    }

    return NC_OK;
}

void NcServerPool::setConf(NcConfPool *_pool)
{
    FUNCTION_INTO(NcServerPool);

    nlive_server = 0;
    next_rebuild = 0LL;

    name = _pool->name;
    addrstr = _pool->listen.pname;
    port = (uint16_t)_pool->listen.port;
    nc_memcpy(&info, &_pool->listen.info, sizeof(_pool->listen.info));
    perm = _pool->listen.perm;

    LOG_DEBUG("info.family : %d, addstr : %s, port : %d", 
        info.family, addrstr.c_str(), port);

    tcpkeepalive = _pool->tcpkeepalive ? 1 : 0;
    timeout = _pool->timeout;
    backlog = _pool->backlog;

    client_connections = (uint32_t)_pool->client_connections;
    server_connections = (uint32_t)_pool->server_connections;
    server_retry_timeout = (int64_t)_pool->server_retry_timeout * 1000LL;
    server_failure_limit = (uint32_t)_pool->server_failure_limit;

    for (uint32_t i = 0; i < _pool->server.size(); i++)
    {
        NcServer *ns = new NcServer(this);
        ns->setConf((_pool->server)[i]);
        server.push_back(ns);
    }
}

NcConn* NcServerPool::getConn(uint8_t *key, uint32_t keylen)
{
    FUNCTION_INTO(NcServerPool);

    uint32_t idx = index(key, keylen);
    LOG_DEBUG("server.size() : %d, idx : %d", server.size(), idx);
    NcServer* s = server[idx % server.size()];
    return s->getConn();
}

rstatus_t NcServerPool::update()
{
    return NcHashKit::update(this);
}

uint32_t NcServerPool::index(uint8_t *key, uint32_t keylen)
{
    FUNCTION_INTO(NcServerPool);

    if (hash_tag.length() > 0)
    {
        uint8_t *tag_start = nc_strchr(key, key + keylen, (hash_tag.c_str())[0]);
        if (tag_start != NULL) 
        {
            uint8_t *tag_end = nc_strchr(tag_start + 1, key + keylen, (hash_tag.c_str())[1]);
            if ((tag_end != NULL) && (tag_end - tag_start > 1)) 
            {
                key = tag_start + 1;
                keylen = (uint32_t)(tag_end - key);
            }
        }
    }

    LOG_DEBUG("server.size() : %d", server.size());

    uint32_t hashv;
    if (server.size() <= 1) 
    {
        hashv = 0;
    }

    LOG_DEBUG("keylen : %d", keylen);

    if (keylen == 0)
    {
        hashv = 0;
    }

    // 计算hash值
    hashv = NcHashKit::hash(key_hash_type, (char*)key, keylen);
    LOG_DEBUG("hashv : %d", hashv);

    return NcHashKit::dispatch(this, hashv);
}

void NcServerConn::ref(void *owner)
{
    NcServer *server = (NcServer*)owner;
    ASSERT(server != NULL);

    rstatus_t status = NcUtil::ncResolve(&server->m_addrstr_, server->m_port_, &server->m_info_);
    if (status != NC_OK) 
    {
        m_err_ = EHOSTDOWN;
        m_done_ = 1;
    }

    m_family_ = server->m_info_.family;
    m_addrlen_ = server->m_info_.addrlen;
    m_addr_ = (struct sockaddr *)&server->m_info_.addr;

    server->m_ns_conn_q_++;
    // 将当前链接插入到server的链接池中
    server->m_conn_queue_.push((NcConnBase*)this);

    m_owner_ = owner;
    LOG_DEBUG("ref conn %p owner %p into '%.*s", this, server,
        server->m_pname_.length(), server->m_pname_.c_str());
}

void NcServerConn::unref()
{
    NcServer *server = (NcServer*)m_owner_;
    ASSERT(server != NULL);
    m_owner_ = NULL;

    server->m_ns_conn_q_--;
    server->m_conn_queue_.remove(this);

    LOG_DEBUG("unref conn %p owner %p into '%.*s", this, server,
              server->m_pname_.length(), server->m_pname_.c_str());
}

bool NcServerConn::active()
{
    FUNCTION_INTO(NcServerConn);

    if (!m_imsg_q_.empty())
    {
        LOG_DEBUG("s %d is active", m_sd_);
        return true;
    }

    if (!m_omsg_q_.empty())
    {
        LOG_DEBUG("s %d is active", m_sd_);
        return true;
    }

    if (m_rmsg_ != NULL)
    {
        LOG_DEBUG("s %d is active", m_sd_);
        return true;
    }

    if (m_smsg_ != NULL)
    {
        LOG_DEBUG("s %d is active", m_sd_);
        return true;
    }

    LOG_DEBUG("s %d is active", m_sd_);
    return false;
}

void NcServerConn::close()
{
    FUNCTION_INTO(NcServerConn);

    // TODO :

    return ;
}

rstatus_t NcServerConn::connect()
{
    FUNCTION_INTO(NcServerConn);

    NcServer *server = (NcServer*)m_owner_;
    ASSERT(server != NULL);

    NcContext *ctx = server->getContext();
    ASSERT(ctx != NULL);

    if (m_err_) 
    {
        LOG_ERROR("err : %d", m_err_);
        errno = m_err_;
        return NC_ERROR;
    }

    if (m_sd_ > 0) 
    {
        LOG_DEBUG("already connected on server connection, sd : %d", m_sd_);
        return NC_OK;
    }

    LOG_DEBUG("connect to server '%.*s'", server->m_name_.length(), 
        server->m_name_.c_str());

    rstatus_t status;

    m_sd_ = ::socket(m_family_, SOCK_STREAM, 0);
    if (m_sd_ < 0) 
    {
        LOG_ERROR("socket for server '%.*s' failed: %s", server->m_name_.length(),
            server->m_name_.c_str(), strerror(errno));
        status = NC_ERROR;
        goto ERROR;
    }

    status = NcUtil::ncSetNonBlocking(m_sd_);
    if (status != NC_OK) 
    {
        LOG_ERROR("set nonblock on s %d for server '%.*s' failed: %s", 
            m_sd_, server->m_name_.length(), server->m_name_.c_str(),
            strerror(errno));
        goto ERROR;
    }

    if ((server->m_name_).c_str()[0] != '/') 
    {
        status = NcUtil::ncSetTcpNodelay(m_sd_);
        if (status != NC_OK) 
        {
            LOG_WARN("set tcpnodelay on s %d for server '%.*s' failed, ignored: %s",
                m_sd_, server->m_name_.length(), server->m_name_.c_str(),
                strerror(errno));
        }
    }

    status = ctx->getEvb().addConn(this);
    if (status != NC_OK)
    {
        LOG_ERROR("event add conn s %d for server '%.*s' failed: %s",
            m_sd_, server->m_name_.length(), server->m_name_.c_str(),
            strerror(errno));
        goto ERROR;
    }

    status = ::connect(m_sd_, m_addr_, m_addrlen_);
    if (status != NC_OK) 
    {
        if (errno == EINPROGRESS) 
        {
            m_connecting_ = 1;
            LOG_DEBUG("connecting on s %d to server '%.*s'",
                m_sd_, server->m_name_.length(), server->m_name_.c_str());
            return NC_OK;
        }

        LOG_ERROR("connect on s %d to server '%.*s' failed: %s", 
            m_sd_, server->m_name_.length(), server->m_name_.c_str(), 
            strerror(errno));

        goto ERROR;
    }

    m_connected_ = 1;
    LOG_DEBUG("connected on s %d to server '%.*s'", m_sd_,
              server->m_name_.length(), server->m_name_.c_str());

    return NC_OK;

ERROR:
    m_err_ = errno;
    return status;
}

NcMsgBase* NcServerConn::sendNext()
{
    FUNCTION_INTO(NcServerConn);

    NcServer *server = (NcServer*)m_owner_;
    ASSERT(server != NULL);

    NcContext *ctx = server->getContext();
    ASSERT(ctx != NULL);

    if (m_connecting_)
    {
        connect();
    }

    rstatus_t status;
    NcQueue<NcMsgBase*>::ConstIterator iter = m_imsg_q_.begin();
    NcMsg *nmsg = (NcMsg*)(*iter);

    LOG_DEBUG("nmsg : %p", nmsg);

    if (nmsg == NULL)
    {
        status = (ctx->getEvb()).delOutput(this);
        if (status != NC_OK)
        {
            m_err_ = errno;
        }

        return NULL;
    }

    NcMsg *msg = (NcMsg*)m_smsg_;
    if (msg != NULL)
    {
        iter++;
        if (iter == m_imsg_q_.end())
        {
            nmsg = NULL;
        }
        else
        {
            nmsg = (NcMsg*)(*iter);
        }
    }

    LOG_DEBUG("nmsg : %p", nmsg);

    m_smsg_ = nmsg;
    if (nmsg == NULL)
    {
        return NULL;
    }

    LOG_DEBUG("send next req %" PRIu64 " len %" PRIu32 " type %d on "
        "s %d", nmsg->m_id_, nmsg->m_mlen_, nmsg->m_type_, m_sd_);

    return nmsg;
}

void NcServerConn::sendDone(NcMsgBase *msg)
{
    FUNCTION_INTO(NcServerConn);

    LOG_DEBUG("send done rsp %" PRIu64 " on c %d", msg->m_id_, m_sd_);

    /* dequeue the message (request) from server inq */
    this->dequeueInput(msg);

    /*
     * noreply request instructs the server not to send any response. So,
     * enqueue message (request) in server outq, if response is expected.
     * Otherwise, free the noreply request
     */
    if (!msg->m_noreply_) 
    {
        this->enqueueOutput(msg);
    } 
    else
    {
        freeMsg(msg);
    }
}

NcMsgBase* NcServerConn::recvNext(bool alloc)
{
    FUNCTION_INTO(NcServerConn);

    NcContext *ctx = (NcContext*)getContext();
    ASSERT(ctx != NULL);

    LOG_DEBUG("[1]conn : %p, m_rmsg_ : %p", this, m_rmsg_);

    NcMsg *msg = (NcMsg*)m_rmsg_;
    if (m_eof_)
    {
        if (msg != NULL)
        {
            m_rmsg_ = NULL;
            LOG_ERROR("eof s %d discarding incomplete rsp %" PRIu64 " len "
                "%" PRIu32 "", m_sd_, msg->m_id_, msg->m_mlen_);
            freeMsg(msg, false);   
        }

        m_done_ = 1;
        LOG_ERROR("s %d active %d is done", m_sd_, active());
        return NULL;
    }

    if (msg != NULL) 
    {
        return msg;
    }

    if (!alloc) 
    {
        return NULL;
    }

    // 分配msg
    msg = (NcMsg*)(ctx->msg_pool).alloc<NcMsg>();
    if (msg != NULL) 
    {
        m_rmsg_ = msg;
    }

    LOG_DEBUG("[2]conn : %p, m_rmsg_ : %p", this, m_rmsg_);

    return msg;
}

void NcServerConn::recvDone(NcMsgBase *cmsg, NcMsgBase *rmsg)
{
    FUNCTION_INTO(NcServerConn);
    
    LOG_DEBUG("conn : %p, rmsg : %p", this, rmsg);
    
    m_rmsg_ = rmsg;
    NcMsg *msg = (NcMsg*)cmsg;
    
    LOG_DEBUG("msg mlen : %d", msg->m_mlen_);

    if (msg->responseFilter(this)) 
    {
        return ;
    }

    msg->responseForward(this);
}

void* NcServerConn::getContext()
{
    NcServer *server = (NcServer*)m_owner_;
    ASSERT(server != NULL);

    return server->getContext();
}