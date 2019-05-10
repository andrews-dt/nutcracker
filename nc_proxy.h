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

    rstatus_t reuse();
    rstatus_t listen();
    rstatus_t accept();
};

#endif