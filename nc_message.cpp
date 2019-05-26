#include <nc_message.h>
#include <nc_client.h>
#include <nc_server.h>

inline NcMbuf* NcMsg::ensureMbuf(NcContext *ctx, size_t len)
{
    NcMbuf *buf = m_mbuf_queue_.front();
    if (buf == NULL || buf->size() >= len)
    {
        buf = (ctx->mbuf_pool).alloc<NcMbuf>();
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
    NcMbuf *buf = (ctx->mbuf_pool).alloc<NcMbuf>();
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
    NcMbuf *buf = (ctx->mbuf_pool).alloc<NcMbuf>();
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

    LOG_DEBUG("[this=%p]m_mbuf_queue_ size : %d", this, m_mbuf_queue_.size());

    rstatus_t status = NC_OK;

    if (empty())
    {
        conn->recvDone(this, NULL);
        return NC_OK;
    }

    // TODO : 测试，默认解析完成
    static int testi = 0;
    if (testi >= 0)
    {
        m_result_ = kMSG_PARSE_OK;
        NcMbuf *mbuf = m_mbuf_queue_.back();
        pos = mbuf->getLast();
    }
    testi++;

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
    ASSERT(ctx != NULL);
    NcMbuf *mbuf = m_mbuf_queue_.back();
    if (mbuf == NULL)
    {
        LOG_ERROR("mbuf is NULL");
        return NC_ENOMEM;
    }

    LOG_DEBUG("mbuf : %p, pos : %p, last : %p", mbuf, pos, mbuf->getLast());
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

    NcMsg *nmsg = (NcMsg *)(ctx->msg_pool).alloc<NcMsg>();
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

    conn->recvDone(this, nmsg);

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

bool NcMsg::requestDone(NcConn* conn)
{
    return true;
}

bool NcMsg::requestFilter(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

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
    if (!m_noreply_) 
    {
        conn->enqueueOutput((NcMsgBase*)this);
    }

    NcServerPool *pool = (NcServerPool*)(conn->m_owner_);
    ASSERT(pool != NULL);
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
    if (nmsg->requestDone(conn)) 
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
    ASSERT(ctx != NULL);

    NcMsg *rsp = (NcMsg*)(ctx->msg_pool).alloc<NcMsg>();
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

bool NcMsg::responseFilter(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

    if (empty()) 
    {
        LOG_DEBUG("filter empty rsp %" PRIu64 " on s %d", m_id_, conn->m_sd_);
        conn->freeMsg(this, false);
        return true;
    }

    LOG_DEBUG("m_omsg_q_ size : %d", conn->m_omsg_q_.size());

    NcMsg *pmsg = (NcMsg*)(conn->m_omsg_q_.front());
    if (pmsg == NULL) 
    {
        LOG_DEBUG("filter stray rsp %" PRIu64 " len %" PRIu32 " on s %d",
            m_id_, m_mlen_, conn->m_sd_);
        conn->freeMsg(this, false);
        /*
         * Memcached server can respond with an error response before it has
         * received the entire request. This is most commonly seen for set
         * requests that exceed item_size_max. IMO, this behavior of memcached
         * is incorrect. The right behavior for update requests that are over
         * item_size_max would be to either:
         * - close the connection Or,
         * - read the entire item_size_max data and then send CLIENT_ERROR
         *
         * We handle this stray packet scenario in nutcracker by closing the
         * server connection which would end up sending SERVER_ERROR to all
         * clients that have requests pending on this server connection. The
         * fix is aggressive, but not doing so would lead to clients getting
         * out of sync with the server and as a result clients end up getting
         * responses that don't correspond to the right request.
         *
         * See: https://github.com/twitter/twemproxy/issues/149
         */
        m_err_ = EINVAL;
        m_done_ = 1;

        return true;
    }

    return false;
}

void NcMsg::responseForward(NcConn* conn)
{
    FUNCTION_INTO(NcMsg);

    NcContext *ctx = (NcContext*)(conn->getContext());
    ASSERT(ctx != NULL);

    uint32_t msgsize = m_mlen_;

    /* dequeue peer message (request) from server */
    NcMsg *pmsg = (NcMsg*)(conn->m_omsg_q_.front());
    conn->dequeueOutput(pmsg);
    pmsg->m_done_ = 1;

    /* establish msg <-> pmsg (response <-> request) link */
    pmsg->m_peer_ = this;
    m_peer_ = pmsg;

    NcConn* c_conn = (NcConn*)(pmsg->data);
    NcMsg* msg = (NcMsg*)(c_conn->m_omsg_q_.front());
    
    LOG_DEBUG("c_conn : %p, m_sd_ : %d, msg : %p, msg->m_mlen_ : %d, pmsg->m_mlen_ : %d", 
        c_conn, c_conn->m_sd_, msg, msg->m_mlen_, pmsg->m_mlen_);

    if (msg != NULL && msg->requestDone(c_conn)) 
    {
        rstatus_t status = (ctx->getEvb()).addOutput(c_conn);
        if (status != NC_OK) 
        {
            c_conn->m_err_ = errno;
        }
    }
}

void NcMsg::freeMbuf(NcContext *ctx)
{
    ASSERT(ctx != NULL);
    
    NcMbuf *mbuf = NULL;
    while (!m_mbuf_queue_.empty())
    {
        mbuf = m_mbuf_queue_.front();
        (ctx->mbuf_pool).free(mbuf);
        m_mbuf_queue_.pop();
    }
}