#include <nc_server.h>
#include <nc_message.h>
#include <nc_hashkit.h>

NcContext* NcServer::getContext()
{
    if (m_server_pool_ == NULL)
    {
        return NULL;
    }

    return m_server_pool_->ctx;
}

NcServerPool* NcServer::getServerPool()
{
    return m_server_pool_;
}

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
        return (NcConn*)(m_server_pool_->ctx->s_pool).alloc();
    }

    NcConnBase *conn = m_conn_queue_.front();
    m_conn_queue_.remove(conn);
    m_conn_queue_.push(conn);

    return (NcConn*)conn;
}

rstatus_t NcServerPool::hashUpdate()
{
    FUNCTION_INTO(NcServerPool);

    switch (dist_type) 
    {
    case kDIST_MODULA:
        return NcHashKit::modula_update(this);

    // 默认使用随机
    case kDIST_RANDOM:
    default:
        return NcHashKit::random_update(this);
    }

    return NC_ERROR;
}

uint32_t NcServerPool::hashIndex(uint8_t *key, uint32_t keylen)
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

    hashv = NcHashKit::hash(key_hash_type, (char*)key, keylen);

    LOG_DEBUG("hashv : %d, ncontinuum : %d", hashv, ncontinuum);
 
    uint32_t idx = 0;
    switch (dist_type) 
    {
    case kDIST_MODULA:
        idx = NcHashKit::modula_dispatch(continuum, ncontinuum, hashv); 
        break;

    // 默认使用随机
    case kDIST_RANDOM:
    default:
        idx = NcHashKit::random_dispatch(continuum, ncontinuum, 0); 
        break;
    }

    return idx;
}

void NcServerConn::ref(void *owner)
{
    NcServer *server = (NcServer*)owner;

    rstatus_t status = NcUtil::nc_resolve(&server->m_addrstr_, server->m_port_, &server->m_info_);
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
    return ;
}

rstatus_t NcServerConn::connect()
{
    FUNCTION_INTO(NcServerConn);

    NcServer *server = (NcServer*)m_owner_;
    if (server == NULL)
    {
        LOG_ERROR("server is NULL");
        return NC_ERROR;
    }

    NcContext *ctx = server->getContext();
    if (ctx == NULL)
    {
        LOG_ERROR("ctx is NULL");
        return NC_ERROR;
    }

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

    status = NcUtil::nc_set_nonblocking(m_sd_);
    if (status != NC_OK) 
    {
        LOG_ERROR("set nonblock on s %d for server '%.*s' failed: %s", 
            m_sd_, server->m_name_.length(), server->m_name_.c_str(),
            strerror(errno));
        goto ERROR;
    }

    if ((server->m_name_).c_str()[0] != '/') 
    {
        status = NcUtil::nc_set_tcpnodelay(m_sd_);
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
    msg = (NcMsg*)(ctx->msg_pool).alloc();
    if (msg != NULL) 
    {
        m_rmsg_ = msg;
    }

    return msg;
}

void NcServerConn::recvDone(NcMsgBase *cmsg, NcMsgBase *rmsg)
{
    m_rmsg_ = rmsg;
    NcMsg *msg = (NcMsg*)cmsg;
    if (msg->responseFilter(this)) 
    {
        return ;
    }

    msg->responseForward(this);
}

NcContext* NcServerConn::getContext()
{
    NcServer *server = (NcServer*)m_owner_;
    if (server == NULL)
    {
        return NULL;
    }

    return server->getContext();
}