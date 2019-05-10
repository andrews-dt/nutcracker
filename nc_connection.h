#ifndef _NC_CONNECTION_H_
#define _NC_CONNECTION_H_

#include <nc_core.h>

/*
 *                   nc_connection.[ch]
 *                Connection (struct conn)
 *                 +         +          +
 *                 |         |          |
 *                 |       Proxy        |
 *                 |     nc_proxy.[ch]  |
 *                 /                    \
 *              Client                Server
 *           nc_client.[ch]         nc_server.[ch]
 *
 * Nutcracker essentially multiplexes m client connections over n server
 * connections. Usually m >> n, so that nutcracker can pipeline requests
 * from several clients over a server connection and hence use the connection
 * bandwidth to the server efficiently
 *
 * Client and server connection maintain two fifo queues for requests:
 *
 * 1). in_q (imsg_q):  queue of incoming requests
 * 2). out_q (omsg_q): queue of outstanding (outgoing) requests
 *
 * Request received over the client connection are forwarded to the server by
 * enqueuing the request in the chosen server's in_q. From the client's
 * perspective once the request is forwarded, it is outstanding and is tracked
 * in the client's out_q (unless the request was tagged as noreply). The server
 * in turn picks up requests from its own in_q in fifo order and puts them on
 * the wire. Once the request is outstanding on the wire, and a response is
 * expected for it, the server keeps track of outstanding requests it in its
 * own out_q.
 *
 * The server's out_q enables us to pair a request with a response while the
 * client's out_q enables us to pair request and response in the order in
 * which they are received from the client.
 *
 *
 *      Clients                             Servers
 *                                    .
 *    in_q: <empty>                   .
 *    out_q: req11 -> req12           .   in_q:  req22
 *    (client1)                       .   out_q: req11 -> req21 -> req12
 *                                    .   (server1)
 *    in_q: <empty>                   .
 *    out_q: req21 -> req22 -> req23  .
 *    (client2)                       .
 *                                    .   in_q:  req23
 *                                    .   out_q: <empty>
 *                                    .   (server2)
 *
 * In the above example, client1 has two pipelined requests req11 and req12
 * both of which are outstanding on the server connection server1. On the
 * other hand, client2 has three requests req21, req22 and req23, of which
 * only req21 is outstanding on the server connection while req22 and
 * req23 are still waiting to be put on the wire. The fifo of client's
 * out_q ensures that we always send back the response of request at the head
 * of the queue, before sending out responses of other completed requests in
 * the queue.
 */

class NcConn : public NcConnBase
{
public:
    NcConn()
    {
        reset();
        LOG_DEBUG("NcConn reset");
    }

    void reset()
    {
        NcConnBase::reset();

        m_owner_ = NULL;
        m_connecting_ = 0;
        m_connected_ = 0;
        m_authenticated_ = 0;

        m_rmsg_ = NULL;
        m_smsg_ = NULL;
    }

    rstatus_t recvChain(NcMsgBase *msg);

    rstatus_t sendChain(NcMsgBase *msg);

    // 收到和发送msg信息
    virtual rstatus_t recvMsg();

    virtual rstatus_t sendMsg();

    virtual void* getContext()
    {
        return NULL;
    }

    virtual int callback(uint32_t events);
    
    virtual void ref(void *owner = NULL) = 0;
    virtual void unref() = 0;
    virtual bool active() = 0;
    virtual void close() = 0;

    // msg入队列和出队列
    virtual void enqueueInput(NcMsgBase *msg) 
    { }

    virtual void dequeueInput(NcMsgBase *msg) 
    { }

    virtual void enqueueOutput(NcMsgBase *msg) 
    { }

    virtual void dequeueOutput(NcMsgBase *msg) 
    { }

    // 其他处理
    virtual NcMsgBase* recvNext(bool alloc)
    {
        return NULL;
    }

    virtual void recvDone(NcMsgBase *msg1, NcMsgBase *msg2)
    { }

    virtual NcMsgBase* sendNext()
    {
        return NULL;
    }

    virtual void sendDone(NcMsgBase *msg)
    { }

    inline int timeout()
    {
        return m_timeout_;
    }

    inline void* getOwner()
    {
        return m_owner_;
    }
    
    // 错误处理函数
    void processError();

    rstatus_t processRecv();

    rstatus_t processSend();

    // 关闭处理
    void processClose();

    // 释放msg
    void freeMsg(NcMsgBase *msg, bool force = true);

    // 处理请求
    bool requestDone(NcMsgBase *msg);

    bool requestError(NcMsgBase *msg);

public:
    void                *m_owner_;          /* connection owner - server_pool / server */

    unsigned            m_connecting_;      /* connecting? */
    unsigned            m_connected_;       /* connected? */
    unsigned            m_authenticated_;   /* authenticated? */
    int                 m_timeout_;

    NcQueue<NcMsgBase*> m_imsg_q_;       /* incoming request queue */
    NcQueue<NcMsgBase*> m_omsg_q_;       /* outstanding request queue */

    NcMsgBase*          m_rmsg_;         /* current message being rcvd */
    NcMsgBase*          m_smsg_;         /* current message being sent */
};
 
#endif
