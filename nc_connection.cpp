#include <nc_connection.h>
#include <nc_server.h>
#include <nc_message.h>

void NcConn::processError()
{
    FUNCTION_INTO(NcConn);

    rstatus_t status = NcUtil::ncGetSoError(m_sd_);
    if (status < 0)
    {
        LOG_WARN("get soerr on %d failed, ignored: %s", m_sd_,
            strerror(errno));
    }
    m_err_ = errno;
    processClose();
}

rstatus_t NcConn::processRecv()
{
    FUNCTION_INTO(NcConn);

    // 调用虚函数
    rstatus_t status = this->recvMsg();
    if (status != NC_OK) 
    {
        LOG_DEBUG("recv on %d failed: %s", m_sd_, strerror(errno));
    }

    return status;
}

rstatus_t NcConn::processSend()
{
    FUNCTION_INTO(NcConn);

    // 调用虚函数
    rstatus_t status = this->sendMsg();
    if (status != NC_OK) 
    {
        LOG_DEBUG("send on %d failed: %s", m_sd_, strerror(errno));
    }

    return status;
}

void NcConn::processClose()
{
    FUNCTION_INTO(NcConn);

    NcContext *ctx = (NcContext*)(this->getContext());

    char *addrstr;

    if (m_conn_type_ == NC_CLIENT) 
    {
        addrstr = NcUtil::ncUnResolvePeerDesc(m_sd_);
    } 
    else 
    {
        addrstr = NcUtil::ncUnresolveAddr(m_addr_, m_addrlen_);
    }

    LOG_DEBUG("close %d", m_sd_);

    rstatus_t status = (ctx->getEvb()).delConn(this);
    if (status < 0) 
    {
        LOG_WARN("event del conn %d failed, ignored: %s", 
            m_sd_, strerror(errno));
    }

    // 调用虚函数
    this->close();
}

int NcConn::callback(uint32_t events)
{
    FUNCTION_INTO(NcConn);

    NcContext *ctx = (NcContext*)(this->getContext());
    if (ctx == NULL)
    {
        LOG_WARN("ctx is NULL");
        return NC_OK;
    }

    LOG_DEBUG("event %04" PRIX32 " on %d", events, m_sd_);

    m_events_ = events;

    if (events & EVENT_ERR) 
    {
        processError();
        return NC_ERROR;
    }

    rstatus_t status;
    if (events & EVENT_READ) 
    {
        status = processRecv();
        if (status != NC_OK || m_done_ || m_err_) 
        {
            processClose();
            return NC_ERROR;
        }
    }

    if (events & EVENT_WRITE) 
    {
        status = processSend();
        if (status != NC_OK || m_done_ || m_err_) 
        {
            processClose();
            return NC_ERROR;
        }
    }
  
    return NC_OK;
}

rstatus_t NcConn::recvMsg()
{
    FUNCTION_INTO(NcConn);

    m_recv_ready_ = 1;
    do 
    {
        NcMsg *msg = (NcMsg*)(this->recvNext(true));
        if (msg == NULL) 
        {
            return NC_OK;
        }

        rstatus_t status = this->recvChain(msg);
        if (status != NC_OK) 
        {
            return status;
        }
    } while (m_recv_ready_);

    return NC_OK;
}

rstatus_t NcConn::sendMsg()
{
    FUNCTION_INTO(NcConn);

    m_send_ready_ = 1;
    do 
    {
        NcMsg *msg = (NcMsg*)(this->sendNext());
        if (msg == NULL) 
        {
            LOG_DEBUG("msg is NULL");
            return NC_OK;
        }

        rstatus_t status = this->sendChain(msg);
        if (status != NC_OK) 
        {
            return status;
        }
    } while (m_send_ready_);

    return NC_OK;
}

rstatus_t NcConn::recvChain(NcMsgBase *_msg)
{
    FUNCTION_INTO(NcConn);

    NcContext *ctx = (NcContext*)getContext();
    ASSERT(ctx != NULL);

    NcMsg *msg = (NcMsg*)_msg;
    NcQueue<NcMbuf*> *mbuf_queue = msg->getMbufQueue();
    NcMbuf *mbuf = mbuf_queue->back();
    if (mbuf == NULL || mbuf->full()) 
    {
        mbuf = (ctx->mbuf_pool).alloc<NcMbuf>();
        if (mbuf == NULL) 
        {
            LOG_ERROR("mbuf is NULL");
            return NC_ENOMEM;
        }

        mbuf_queue->push(mbuf);
        msg->setPos(mbuf->getPos());

        LOG_DEBUG("push mbuf, pos : %p, size : %d", mbuf->getPos(), mbuf_queue->size());
    }

    size_t msize = mbuf->size();
    ssize_t n = recv(mbuf->getLast(), msize);
    LOG_DEBUG("recv : %s, length : %d, msize : %d", mbuf->getLast(), n, msize);
    if (n < 0) 
    {
        if (n == NC_EAGAIN) 
        {
            return NC_OK;
        }

        return NC_ERROR;
    }

    mbuf->setLast(n);
    msg->setLength(n);

    NcMsg *nmsg = NULL;
    for (;;) 
    {
        rstatus_t status = msg->parse(this);
        LOG_DEBUG("status : %d", status);
        if (status != NC_OK)
        {
            return status;
        }

        nmsg = (NcMsg*)(this->recvNext(false));
        if (nmsg == NULL || nmsg == msg) 
        {
            break;
        }

        msg = nmsg;
    }

    return NC_OK;
}

rstatus_t NcConn::sendChain(NcMsgBase *msg)
{
    FUNCTION_INTO(NcConn);

    NcQueue<NcMsg*> send_msgq;
    struct iovec iov[NC_IOV_MAX];
    size_t nsend = 0, nsent = 0, limit = SSIZE_MAX; /* bytes to send; bytes sent */
    size_t mlen = 0, iov_n = 0;
    ssize_t n; 

    NcContext *ctx = (NcContext*)getContext();
    ASSERT(ctx != NULL);
    
    LOG_DEBUG("msg : %p", msg);

    NcMsg *cmsg = (NcMsg*)msg;
    NcQueue<NcMbuf*> *mbuf_queue = cmsg->getMbufQueue();
    for (;;)
    {
        send_msgq.push(cmsg);

        NcMbuf *mbuf = mbuf_queue->front();
        NcQueue<NcMbuf*>::ConstIterator iter = mbuf_queue->find(mbuf);
        while (iter != mbuf_queue->end() && nsend < limit)
        {
            LOG_DEBUG("mbuf_queue size : %d", mbuf_queue->size());
            if ((*iter)->empty())
            {
                iter++;
                continue;
            }

            mlen = (*iter)->length();
            LOG_DEBUG("mlen : %d, pos : %s", mlen, (*iter)->getPos());
            if ((nsend + mlen) > limit)
            {
                mlen = limit - nsend;
            }

            iov[iov_n].iov_base = (*iter)->getPos();
            iov[iov_n].iov_len = mlen;
            iov_n++;

            nsend += mlen;
            iter++;
        }

        LOG_DEBUG("iov_n : %d, NC_IOV_MAX : %d, nsend : %d, limit : %ld", 
            iov_n, NC_IOV_MAX, nsend, limit);

        if (iov_n >= NC_IOV_MAX || nsend > limit)
        {
            break;
        }

        cmsg = (NcMsg*)(this->sendNext());
        if (cmsg == NULL)
        {
            break;
        }
    }

    m_smsg_ = NULL;
    if (!send_msgq.empty() && nsend != 0)
    {
        n = sendv(iov, iov_n); // 链式发送
    }
    else
    {
        n = 0;
    }

    LOG_DEBUG("n : %d", n);

    nsent = n > 0 ? (size_t)n : 0;
    NcMsg *nmsg = send_msgq.front();
    NcQueue<NcMsg*>::ConstIterator iter = send_msgq.find(nmsg);
    while (!send_msgq.empty() && iter != send_msgq.end())
    {
        LOG_DEBUG("send_msgq size : %d, *iter : %p", send_msgq.size(), *iter);
        nmsg = *iter;
        send_msgq.remove(*iter++);

        if (nsent == 0)
        {
            if (nmsg->m_mlen_ == 0)
            {
                this->sendDone(nmsg);
            }
            continue;
        }

        NcQueue<NcMbuf*> *_mbuf_queue = nmsg->getMbufQueue();
        NcMbuf *mbuf = NULL;
        NcQueue<NcMbuf*>::ConstIterator iter1 = _mbuf_queue->begin();
        while (iter1 != _mbuf_queue->end())
        {
            mbuf = *iter1;

            LOG_DEBUG("_mbuf_queue size : %d", _mbuf_queue->size());

            if ((*iter1)->empty())
            {
                iter1++;
                continue;
            }

            mlen = (*iter1)->length();
            if (nsent < mlen)
            {
                mbuf->setPos(nsent);
                nsent = 0;
                break;
            }

            mbuf->setComplete();
            nsent -= mlen;
            iter1++;
        }

        LOG_DEBUG("mbuf : %p", mbuf);

        if (iter1 == _mbuf_queue->end() || mbuf == NULL)
        {
            this->sendDone(nmsg);
        }
    }
    
    if (n > 0)
    {
        return NC_OK;
    }

    return (n == NC_EAGAIN) ? NC_OK : NC_ERROR;
}

void NcConn::freeMsg(NcMsgBase *msg, bool force)
{
    FUNCTION_INTO(NcConn);

    if (m_owner_ == NULL) 
    {
        LOG_WARN("conn is already unrefed, m_owner_ is NULL!");
        return ;
    }

    NcContext *ctx = (NcContext*)(this->getContext());

    if (force)
    {
        NcMsgBase* pmsg = msg->m_peer_;
        if (pmsg != NULL)
        {
            msg->m_peer_ = NULL;
            pmsg->m_peer_ = NULL;
            ((NcMsg*)pmsg)->freeMbuf(ctx);
            (ctx->msg_pool).free(pmsg);
        }

        // 从红黑树删除节点
        (ctx->getRBTree()).remove(msg);
    }

    ((NcMsg*)msg)->freeMbuf(ctx);
    (ctx->msg_pool).free(msg);   
}

bool NcConn::requestDone(NcMsgBase *msg)
{
    return true;
}

bool NcConn::requestError(NcMsgBase *msg)
{
    return true;
}