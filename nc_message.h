#ifndef _NC_MESSAGE_H_
#define _NC_MESSAGE_H_

#include <nc_core.h>
#include <nc_connection.h>

/*
 *            nc_message.[ch]
 *         message (struct msg)
 *            +        +            .
 *            |        |            .
 *            /        \            .
 *         Request    Response      .../ nc_mbuf.[ch]  (mesage buffers)
 *      nc_request.c  nc_response.c .../ nc_memcache.c; nc_redis.c (message parser)
 *
 * Messages in nutcracker are manipulated by a chain of processing handlers,
 * where each handler is responsible for taking the input and producing an
 * output for the next handler in the chain. This mechanism of processing
 * loosely conforms to the standard chain-of-responsibility design pattern
 *
 * At the high level, each handler takes in a message: request or response
 * and produces the message for the next handler in the chain. The input
 * for a handler is either a request or response, but never both and
 * similarly the output of an handler is either a request or response or
 * nothing.
 *
 * Each handler itself is composed of two processing units:
 *
 * 1). filter: manipulates output produced by the handler, usually based
 *     on a policy. If needed, multiple filters can be hooked into each
 *     location.
 * 2). forwarder: chooses one of the backend servers to send the request
 *     to, usually based on the configured distribution and key hasher.
 *
 * Handlers are registered either with Client or Server or Proxy
 * connections. A Proxy connection only has a read handler as it is only
 * responsible for accepting new connections from client. Read handler
 * (conn_recv_t) registered with client is responsible for reading requests,
 * while that registered with server is responsible for reading responses.
 * Write handler (conn_send_t) registered with client is responsible for
 * writing response, while that registered with server is responsible for
 * writing requests.
 *
 * Note that in the above discussion, the terminology send is used
 * synonymously with write or OUT event. Similarly recv is used synonymously
 * with read or IN event
 *
 *             Client+             Proxy           Server+
 *                              (nutcracker)
 *                                   .
 *       msg_recv {read event}       .       msg_recv {read event}
 *         +                         .                         +
 *         |                         .                         |
 *         \                         .                         /
 *         req_recv_next             .             rsp_recv_next
 *           +                       .                       +
 *           |                       .                       |       Rsp
 *           req_recv_done           .           rsp_recv_done      <===
 *             +                     .                     +
 *             |                     .                     |
 *    Req      \                     .                     /
 *    ===>     req_filter*           .           *rsp_filter
 *               +                   .                   +
 *               |                   .                   |
 *               \                   .                   /
 *               req_forward-//  (a) . (c)  \\-rsp_forward
 *                                   .
 *                                   .
 *       msg_send {write event}      .      msg_send {write event}
 *         +                         .                         +
 *         |                         .                         |
 *    Rsp' \                         .                         /     Req'
 *   <===  rsp_send_next             .             req_send_next     ===>
 *           +                       .                       +
 *           |                       .                       |
 *           \                       .                       /
 *           rsp_send_done-//    (d) . (b)    //-req_send_done
 *
 *
 * (a) -> (b) -> (c) -> (d) is the normal flow of transaction consisting
 * of a single request response, where (a) and (b) handle request from
 * client, while (c) and (d) handle the corresponding response from the
 * server.
 */

class NcKeypos 
{
public:
    uint8_t     *start;     /* key start pos */
    uint8_t     *end;       /* key end pos */
};

typedef enum 
{
    kMSG_PARSE_OK,           /* parsing ok */
    kMSG_PARSE_ERROR,        /* parsing error */
    kMSG_PARSE_REPAIR,       /* more to parse -> repair parsed & unparsed data */
    kMSG_PARSE_AGAIN,        /* incomplete -> parse again */
} NcMsgParseResult;

class NcMsg : public NcMsgBase
{
public:
    inline NcMbuf* ensureMbuf(NcContext *ctx, size_t len);

    rstatus_t append(NcContext *ctx, uint8_t *pos, size_t n);

    rstatus_t preAppend(NcContext *ctx, uint8_t *pos, size_t n);

    rstatus_t prependFormat(NcContext *ctx, const char *fmt, ...);

    void dump(NcContext *ctx, int level);

    inline NcQueue<NcMbuf*>* getMbufQueue()
    {
        return &m_mbuf_queue_;
    }

    rstatus_t parse(NcConn* conn);

    rstatus_t parseDone(NcConn* conn);
    
    rstatus_t repairDone(NcConn* conn);

    rstatus_t reply();

    // 处理request
    bool requestFilter(NcConn* conn);

    void requestForward(NcConn* conn);

    void requestForwardError(NcConn* conn);

    rstatus_t requestMakeReply(NcConn* conn);

    // 处理response
    bool responseFilter(NcConn* conn);

    void responseForward(NcConn* conn);

    void freeMbuf(NcContext *ctx);

private:
    NcQueue<NcMbuf*>    m_mbuf_queue_;
    NcMsgParseResult    m_result_;
};

#endif
