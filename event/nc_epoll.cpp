#include <nc_event.h>

#ifdef NC_HAVE_EPOLL
#include <sys/epoll.h>

NcEventBase::NcEventBase(int nevent)
{
    int status, ep;
    struct epoll_event *event;

    ep = epoll_create(nevent);
    if (ep < 0) 
    {
        exit(0);
    }

    event = (struct epoll_event*)calloc(nevent, sizeof(struct epoll_event));
    if (event == NULL) 
    {
        status = ::close(ep);
        if (status < 0) 
        {
            LOG_ERROR("calloc error!!!");
            exit(0);
        }
    }

    m_event_ = (struct _event*)malloc(sizeof(struct _event));
    if (m_event_ == NULL) 
    {
        ::free(event);
        status = ::close(ep);
        if (status < 0) 
        {
            LOG_ERROR("malloc error!!!");
            exit(0);
        }
    }

    m_event_->ep = ep;
    m_event_->event = event;
    m_event_->nevent = nevent;
}

NcEventBase::~NcEventBase()
{
    int status;

    if (m_event_ == NULL) 
    {
        return ;
    }

    ::free(m_event_->event);

    status = ::close(m_event_->ep);
    if (status < 0) 
    {
        LOG_ERROR("close error, ep:%d", m_event_->ep);
    }

    m_event_->ep = -1;
    ::free(m_event_);
}

int NcEventBase::addInput(NcConnBase *c)
{
    int status;
    struct epoll_event event;
    int ep = m_event_->ep;

    if (c->m_recv_active_) 
    {
        return 0;
    }

    event.events = (uint32_t)(EPOLLIN | EPOLLET);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->m_sd_, &event);
    if (status < 0) 
    {
        LOG_ERROR("epoll_ctl error, status:%d", status);
    } 
    else 
    {
        c->m_recv_active_ = 1;
    }

    return status;
}

int NcEventBase::delInput(NcConnBase *c)
{
    LOG_DEBUG("c : %p", c);
    return 0;
}

int NcEventBase::addOutput(NcConnBase *c)
{
    int status;
    struct epoll_event event;
    int ep = m_event_->ep;

    if (c->m_send_active_) 
    {
        return 0;
    }

    event.events = (uint32_t)(EPOLLIN | EPOLLOUT | EPOLLET);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->m_sd_, &event);
    if (status < 0) 
    {
        LOG_ERROR("epoll_ctl error, status:%d", status);
    } 
    else 
    {
        c->m_send_active_ = 1;
    }

    return status;
}

int NcEventBase::delOutput(NcConnBase *c)
{
    int status;
    struct epoll_event event;
    int ep = m_event_->ep;

    if (!c->m_send_active_) 
    {
        return 0;
    }

    event.events = (uint32_t)(EPOLLIN | EPOLLET);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_MOD, c->m_sd_, &event);
    if (status < 0) 
    {
        LOG_ERROR("epoll_ctl error, status:%d", status);
    } 
    else 
    {
        c->m_send_active_ = 0;
    }

    return status;
}

int NcEventBase::addConn(NcConnBase *c, uint32_t events)
{
    int status;
    struct epoll_event event;
    int ep = m_event_->ep;

    uint32_t _es = 0;
    if (events & EVENT_READ)
    {
        _es |= EPOLLIN;
    }

    if (events & EVENT_WRITE)
    {
        _es |= EPOLLOUT;
    }
    
    event.events = (uint32_t)(_es | EPOLLET);
    event.data.ptr = c;

    status = epoll_ctl(ep, EPOLL_CTL_ADD, c->m_sd_, &event);
    if (status < 0) 
    {
        LOG_ERROR("epoll_ctl error, status:%d", status);
    } 
    else 
    {
        c->m_send_active_ = 1;
        c->m_recv_active_ = 1;
    }

    return status;
}

int NcEventBase::delConn(NcConnBase *c)
{
    int status;
    int ep = m_event_->ep;

    status = epoll_ctl(ep, EPOLL_CTL_DEL, c->m_sd_, NULL);
    if (status < 0) 
    {
        LOG_ERROR("epoll_ctl error, status:%d", status);
    } 
    else 
    {
        c->m_recv_active_ = 0;
        c->m_send_active_ = 0;
    }

    return status;
}

int NcEventBase::wait(int timeout)
{
    int ep = m_event_->ep;
    struct epoll_event *event = m_event_->event;
    int nevent = m_event_->nevent;

    for (;;) 
    {
        int i, nsd;

        nsd = epoll_wait(ep, event, nevent, timeout);
        LOG_DEBUG("nsd : %d", nsd);

        if (nsd > 0) 
        {
            for (i = 0; i < nsd; i++) 
            {
                struct epoll_event *ev = &(m_event_->event[i]);
                uint32_t events = 0;

                if (ev->events & EPOLLERR) 
                {
                    events |= EVENT_ERR;
                }

                if (ev->events & (EPOLLIN | EPOLLHUP)) 
                {
                    events |= EVENT_READ;
                }

                if (ev->events & EPOLLOUT) 
                {
                    events |= EVENT_WRITE;
                }

                NcConnBase *c = (NcConnBase*)(ev->data.ptr);
                LOG_DEBUG("ev->events : %d, events : %d, c : %p", 
                    ev->events, events, c);
                if (NULL != c) 
                {
                    c->callback(events);
                }
            }

            return nsd;
        }

        if (nsd == 0) 
        {
            if (timeout == -1) 
            {
                return -1;
            }

            return 0;
        }

        if (errno == EINTR) 
        {
            continue;
        }

        return -1;
    }
}

#endif