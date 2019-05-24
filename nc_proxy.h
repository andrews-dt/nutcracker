#ifndef _NC_PROXY_H_
#define _NC_PROXY_H_

#include <nc_core.h>
#include <nc_connection.h>

class NcProxyConn : public NcConn
{
public:
    virtual void ref(void *owner = NULL);
    virtual void unref();
    virtual bool active();
    virtual void close();
    
    virtual rstatus_t recvMsg();
    virtual rstatus_t sendMsg();

    virtual void* getContext();

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

    rstatus_t reuse();
    rstatus_t listen();
    rstatus_t accept();
};

#endif