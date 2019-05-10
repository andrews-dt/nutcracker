#include <nc_message.h>
#include <nc_client.h>
#include <nc_server.h>

inline NcMbuf* NcMsg::ensureMbuf(NcContext *ctx, size_t len)
{
    NcMbuf *buf = m_mbuf_queue_.front();
    if (buf == NULL || buf->size() >= len)
    {
        buf = (ctx->mbuf_pool).alloc();
    }
    
    if (buf != NULL)
    {
        m_mbuf_queue_.push(buf);
    }

    return buf;
}

rstatus_t NcMsg::append(NcContext *ctx, uint8_t *pos, size_t n)
{
    NcMbuf *buf = ensureMbuf(ctx, n);
    if (buf == NULL)
    {
        LOG_WARN("buf is NULL");
        return NC_ENOMEM;
    }

    buf->copy(pos, n);
    m_mlen_ += (uint32_t)n;

    return NC_OK;
}

rstatus_t NcMsg::preAppend(NcContext *ctx, uint8_t *pos, size_t n)
{
    NcMbuf *buf = (ctx->mbuf_pool).alloc();
    if (buf == NULL)
    {
        LOG_WARN("buf is NULL");
        return NC_ENOMEM;
    }

    buf->copy(pos, n);
    m_mbuf_queue_.push(buf);
    m_mlen_ += (uint32_t)n;

    return NC_OK;
}

rstatus_t NcMsg::prependFormat(NcContext *ctx, const char *fmt, ...)
{
    va_list args;
    NcMbuf *buf = (ctx->mbuf_pool).alloc();
    if (buf == NULL)
    {
        LOG_WARN("buf is NULL");
        return NC_ENOMEM;
    }
    
    va_start(args, fmt);
    uint32_t n = buf->vsnprintf(fmt, args);
    va_end(args);

    if (n <= 0) 
    {
        return NC_ERROR;
    }
    m_mbuf_queue_.push(buf);
    m_mlen_ += (uint32_t)n;

    return NC_OK;
}

rstatus_t NcMsg::parse(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

    LOG_DEBUG("m_mbuf_queue_ size : %d", m_mbuf_queue_.size());

    rstatus_t status = NC_OK;

    if (empty())
    {
        conn->recvDone(this, NULL);
        return NC_OK;
    }

    // TODO : 测试
    conn->m_eof_ = 1;
    m_result_ = kMSG_PARSE_OK;

    switch (m_result_)
    {
    case kMSG_PARSE_OK:
        status = parseDone(conn);
        break;

    case kMSG_PARSE_REPAIR:
        status = repairDone(conn);
        break;

    case kMSG_PARSE_AGAIN:
        status = NC_OK;
        break;

    default:
        status = NC_ERROR;
        conn->m_err_ = errno;
        break;
    }

    LOG_DEBUG("conn->m_err_ : %d, status : %d", conn->m_err_, status);

    return conn->m_err_ != 0 ? NC_ERROR : status;
}

rstatus_t NcMsg::parseDone(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

    NcContext *ctx = (NcContext*)(conn->getContext());
    NcMbuf *mbuf = m_mbuf_queue_.back();
    if (mbuf == NULL)
    {
        LOG_ERROR("mbuf is NULL");
        return NC_ENOMEM;
    }

    if (pos == mbuf->getLast()) 
    {
        conn->recvDone(this, NULL);
        return NC_OK;
    }

    LOG_DEBUG("split mbuf, pos : %p, mbuf length : %d", pos, mbuf->length());

    /*
     * Input mbuf has un-parsed data. Split mbuf of the current message msg
     * into (mbuf, nbuf), where mbuf is the portion of the message that has
     * been parsed and nbuf is the portion of the message that is un-parsed.
     * Parse nbuf as a new message nmsg in the next iteration.
     */
    NcMbuf *nbuf = mbuf->split(pos);
    if (nbuf == NULL) 
    {
        LOG_DEBUG("nbuf is NULL");
        return NC_ENOMEM;
    }
    
    LOG_DEBUG("nbuf length : %d, len : %d", nbuf->length(), m_mlen_);

    NcMsg *nmsg = (NcMsg *)(ctx->msg_pool).alloc();
    if (nmsg == NULL) 
    {
        LOG_DEBUG("nmsg is NULL");
        (ctx->mbuf_pool).free(nbuf);
        return NC_ENOMEM;
    }

    nmsg->m_mbuf_queue_.push(nbuf);
    nmsg->pos = nbuf->getPos();

    /* update length of current (msg) and new message (nmsg) */
    nmsg->m_mlen_ = nbuf->length();
    m_mlen_ -= nmsg->m_mlen_;
    
    LOG_DEBUG("len : %d", m_mlen_);
    
    // TODO : 特殊处理
    conn->recvDone(nmsg, this);

    return NC_OK;
}

rstatus_t NcMsg::repairDone(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);
     
    NcMbuf *mbuf = m_mbuf_queue_.back();
    if (mbuf == NULL)
    {
        return NC_ERROR;
    }

    NcMbuf *nbuf = mbuf->split(pos);
    if (nbuf == NULL) 
    {
        LOG_DEBUG("nbuf is NULL");
        return NC_ENOMEM;
    }

    m_mbuf_queue_.push(nbuf);
    pos = nbuf->getPos();

    return NC_OK;
}

bool NcMsg::requestFilter(NcConn* conn)
{
    if (empty())
    {
        LOG_DEBUG("filter empty req %" PRIu64 " from c %d", m_id_, conn->m_sd_);
        conn->freeMsg(this);
        return true;
    }

    /*
     * Handle "quit\r\n" (memcache) or "*1\r\n$4\r\nquit\r\n" (redis), which
     * is the protocol way of doing a passive close. The connection is closed
     * as soon as all pending replies have been written to the client.
     */
    if (m_quit_)
    {
        LOG_DEBUG("filter quit req %" PRIu64 " from c %d", m_id_, conn->m_sd_);
        
        if (conn->m_rmsg_ != NULL)
        {
            LOG_DEBUG("discard invalid req %" PRIu64 " len %" PRIu32 " "
                "from c %d sent after quit req", conn->m_rmsg_->m_id_,
                conn->m_rmsg_->m_mlen_, conn->m_sd_);
        }

        conn->m_eof_ = 1;
        conn->m_recv_ready_ = 0;

        conn->freeMsg(this);
        return true;
    }

    // TODO : 判断是否需要鉴权

    return false;
}

void NcMsg::requestForward(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

    NcContext *ctx = (NcContext*)(conn->getContext());
    /* enqueue message (request) into client outq, if response is expected */
    if (!m_noreply_) 
    {
        conn->enqueueOutput((NcMsgBase*)this);
    }

    NcServerPool *pool = (NcServerPool*)(conn->m_owner_);

    // TODO : 需要特殊处理
    NcConn *s_conn = pool->getConn(NULL, 0);
    if (s_conn == NULL) 
    {
        requestForwardError(conn);
        return ;
    }

    LOG_DEBUG("s_conn size : %d, mlen : %d", s_conn->m_imsg_q_.size(), m_mlen_);

    /* enqueue the message (request) into server inq */
    if (s_conn->m_imsg_q_.empty()) 
    {
        rstatus_t status = (ctx->getEvb()).addOutput(s_conn);
        if (status != NC_OK) 
        {
            requestForwardError(conn);
            s_conn->m_err_ = errno;
            return ;
        }
    }

    s_conn->enqueueInput(this);

    LOG_DEBUG("forward from c %d to s %d req %" PRIu64 " len %" PRIu32
        " type %d", conn->m_sd_, s_conn->m_sd_, m_id_, m_mlen_, m_type_);
}

void NcMsg::requestForwardError(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

    NcContext *ctx = (NcContext*)(conn->getContext());
    LOG_DEBUG("forward req %" PRIu64 " len %" PRIu32 " type %d from "
              "c %d failed: %s", m_id_, m_mlen_, m_type_, conn->m_sd_,
              strerror(errno));

    m_done_ = 1;
    m_error_ = 1;
    m_err_ = errno;

    /* noreply request don't expect any response */
    if (m_noreply_) 
    {
        conn->freeMsg(this);
        return ;
    }

    NcMsg* nmsg = (NcMsg*)(conn->m_omsg_q_).front();
    if (conn->requestDone(nmsg)) 
    {
        rstatus_t status = (ctx->getEvb()).addOutput(conn);
        if (status != NC_OK) 
        {
            m_err_ = errno;
        }
    }
    
    return ;
}

rstatus_t NcMsg::requestMakeReply(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

    NcContext *ctx = (NcContext*)(conn->getContext());

    NcMsg *rsp = (NcMsg*)(ctx->msg_pool).alloc();
    if (rsp == NULL) 
    {
        m_err_ = errno;
        return NC_ENOMEM;
    }

    m_peer_ = (NcMsgBase*)rsp;
    rsp->m_peer_ = (NcMsg*)this;
    m_done_ = 1;
    conn->enqueueOutput(this);

    return NC_OK;
}

void NcMsg::freeMbuf(NcContext *ctx)
{
    if (ctx == NULL)
    {
        return ;
    }
    
    NcMbuf *mbuf = NULL;
    while (!m_mbuf_queue_.empty())
    {
        mbuf = m_mbuf_queue_.front();
        (ctx->mbuf_pool).free(mbuf);
        m_mbuf_queue_.pop();
    }
}