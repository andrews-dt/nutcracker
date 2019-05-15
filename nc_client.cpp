#include <nc_util.h>
#include <nc_message.h>
#include <nc_client.h>
#include <nc_server.h>

void NcClientConn::ref(void *owner)
{
    FUNCTION_INTO(NcClientConn);

    m_owner_ = owner;
    NcServerPool *pool = (NcServerPool*)m_owner_;

    m_family_ = 0;
    m_addrlen_ = 0;
    m_addr_ = NULL;

    pool->nc_conn_q++;
    (pool->c_conn_q).push((NcConn*)this);
    LOG_DEBUG("ref conn %p owner %p into pool '%.*s'", this, pool,
        pool->name.length(), pool->name.c_str());
}

void NcClientConn::unref()
{
    FUNCTION_INTO(NcClientConn);

    NcServerPool *pool = (NcServerPool*)m_owner_;
    m_owner_ = NULL;
    if (pool->nc_conn_q <= 0)
    {
        LOG_ERROR("pool->nc_conn_q nums : %d", pool->nc_conn_q);
    }
    pool->nc_conn_q--;
    // 移除指定的元素
    (pool->c_conn_q).remove(this);
    LOG_DEBUG("unref conn %p owner %p into pool '%.*s'", this, pool,
        pool->name.length(), pool->name.c_str());
}

bool NcClientConn::active()
{
    FUNCTION_INTO(NcClientConn);

    if (!m_imsg_q_.empty())
    {
        LOG_DEBUG("c %d is active", m_sd_);
        return true;
    }

    if (m_rmsg_ != NULL)
    {
        LOG_DEBUG("c %d is active", m_sd_);
        return true;
    }

    if (m_smsg_ != NULL)
    {
        LOG_DEBUG("c %d is active", m_sd_);
        return true;
    }

    LOG_DEBUG("c %d is active", m_sd_);
    return false;
}

void NcClientConn::close()
{
    FUNCTION_INTO(NcClientConn);

    NcContext *ctx = (NcContext*)getContext();
    ASSERT(ctx != NULL);

    if (m_sd_ < 0)
    {
        unref();
        (ctx->c_pool).free(this);
        return ;
    }

    NcMsgBase *msg = m_rmsg_;
    if (msg != NULL)
    {
        m_rmsg_ = NULL;
        LOG_DEBUG("close c %d discarding pending req %" PRIu64 " len "
                  "%" PRIu32 " type %d", m_sd_, msg->m_id_, msg->m_mlen_,
                  msg->m_type_);
        freeMsg(msg);
    }

    for (NcQueue<NcMsgBase*>::ConstIterator iter = m_omsg_q_.begin(); 
        iter != m_omsg_q_.end(); iter++)
    {
        msg = *iter;

        LOG_DEBUG("close c %d discarding pending req %" PRIu64 " len "
            "%" PRIu32 " type %d", m_sd_, msg->m_id_, msg->m_mlen_,
            msg->m_type_);
        if (msg->m_done_)
        {   
            LOG_DEBUG("close c %d discarding %s req %" PRIu64 " len "
                "%" PRIu32 " type %d", m_sd_,
                msg->m_error_ ? "error": "completed", msg->m_id_, 
                msg->m_mlen_, msg->m_type_);
            freeMsg(msg);
        }
        else
        {
            msg->m_swallow_ = 1;
            LOG_DEBUG("close c %d discarding %s req %" PRIu64 " len "
                "%" PRIu32 " type %d", m_sd_,
                msg->m_error_ ? "error": "completed", msg->m_id_, 
                msg->m_mlen_, msg->m_type_);
        }
    }

    unref();
    
    rstatus_t status = ::close(m_sd_);
    if (status < 0)
    {
        LOG_ERROR("close c %d failed, ignored: %s", m_sd_, strerror(errno));
    }
    
    m_sd_ = -1;
    (ctx->c_pool).free(this);
}

NcMsgBase* NcClientConn::recvNext(bool alloc)
{
    FUNCTION_INTO(NcClientConn);

    NcContext *ctx = (NcContext*)getContext();
    ASSERT(ctx != NULL);

    NcMsg *msg = (NcMsg*)m_rmsg_;
    if (m_eof_) 
    {
        /* 服务器在发送整个请求之前发送了EOF */
        if (msg != NULL) 
        {
            m_rmsg_ = NULL;
            LOG_ERROR("eof c %d discarding incomplete rsp %" PRIu64 " len "
                      "%" PRIu32 "", m_sd_, msg->m_id_, msg->m_mlen_);
            freeMsg(msg);
        }
        /*
         * We treat TCP half-close from a server different from how we treat
         * those from a client. On a FIN from a server, we close the connection
         * immediately by sending the second FIN even if there were outstanding
         * or pending requests. This is actually a tricky part in the FA, as
         * we don't expect this to happen unless the server is misbehaving or
         * it crashes
         */
        if (!this->active())
        {
            m_done_ = 1;
            LOG_DEBUG("c %d is done", m_sd_);
        }

        return NULL;
    }

    LOG_DEBUG("msg : %p", msg);
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
    if (msg == NULL)
    {
        m_err_ = errno;
        return NULL;
    }
    m_rmsg_ = (NcMsg*)msg;
    // 设置所属数据
    msg->setData(this);
    return (NcMsgBase*)msg;
}

void NcClientConn::recvDone(NcMsgBase *cmsg, NcMsgBase *rmsg)
{
    FUNCTION_INTO(NcClientConn);
    NcQueue<NcMsg*> frag_msgq;

    NcContext *ctx = (NcContext*)getContext();
    ASSERT(ctx != NULL);
    ASSERT(cmsg != NULL);

    m_rmsg_ = rmsg;
    if (((NcMsg*)cmsg)->requestFilter(this))
    {
        return ;
    }

    rstatus_t status;
    if (cmsg->m_noforward_)
    {
        status = ((NcMsg*)cmsg)->requestMakeReply(this);
        if (status != NC_OK) 
        {
            m_err_ = errno;
            return;
        }

        status = ((NcMsg*)cmsg)->reply();
        if (status != NC_OK) 
        {
            m_err_ = errno;
            return;
        }

        status = (ctx->getEvb()).addOutput(this);
        if (status != NC_OK) 
        {
            m_err_ = errno;
        }

        return;
    }

    if (frag_msgq.empty())
    {
        ((NcMsg*)cmsg)->requestForward(this);
        return ;
    }

    // TODO ：处理
    // rstatus_t status = (ctx->getEvb()).addOutput(this);
    // if (status != NC_OK) 
    // {
    //     m_err_ = errno;
    // }

    return ;
}

NcMsgBase* NcClientConn::sendNext()
{
    FUNCTION_INTO(NcClientConn);

    NcServerPool *pool = (NcServerPool*)m_owner_;
    NcContext *ctx = pool->ctx;
    if (ctx == NULL)
    {
        LOG_ERROR("ctx is NULL");
        return NULL;
    }

    rstatus_t status;
    NcMsg *pmsg = (NcMsg*)(m_omsg_q_.front());

    // TODO : 需要特殊处理
    if (pmsg == NULL)
    {
        if (pmsg == NULL && m_eof_)
        {
            m_done_ = 1;
            LOG_DEBUG("c %d is done", m_sd_);
        }

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
        pmsg = NULL;
        // TODO : 需要特殊处理
    }

    if (pmsg == NULL)
    {
        m_smsg_ = NULL;
    }

    // TODO : 需要特殊处理
    m_smsg_ = msg;

    LOG_DEBUG("send next rsp on c %d", m_sd_);

    return msg;
}

void NcClientConn::sendDone(NcMsgBase *msg)
{
    LOG_DEBUG("send done rsp %" PRIu64 " on c %d", msg->m_id_, m_sd_);

    NcMsg *pmsg = (NcMsg*)(msg->m_peer_);
    dequeueOutput(pmsg);

    freeMsg(pmsg);
}

void* NcClientConn::getContext()
{
    NcServerPool *pool = (NcServerPool*)m_owner_;
    if (pool == NULL)
    {
        return NULL;
    }

    NcContext *ctx = pool->ctx;
    if (ctx == NULL)
    {
        LOG_ERROR("ctx is NULL");
        return NULL;
    }

    return ctx;
}