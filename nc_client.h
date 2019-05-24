#ifndef _NC_CLIENT_H_
#define _NC_CLIENT_H_

#include <nc_connection.h>

class NcClientConn : public NcConn
{
public:
    virtual void ref(void *owner = NULL);
    virtual void unref();
    virtual bool active();
    virtual void close();

    virtual void enqueueOutput(NcMsgBase *msg)
    {
        FUNCTION_INTO(NcClientConn);
        m_omsg_q_.push(msg);
    }

    virtual void dequeueOutput(NcMsgBase *msg)
    {
        FUNCTION_INTO(NcClientConn);
        m_omsg_q_.remove(msg);
    }

    virtual NcMsgBase* recvNext(bool alloc);

    virtual void recvDone(NcMsgBase *cmsg, NcMsgBase *rmsg);

    virtual NcMsgBase* sendNext();

    virtual void sendDone(NcMsgBase *msg);

    virtual void* getContext();
};

#endif
