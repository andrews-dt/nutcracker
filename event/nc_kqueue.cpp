#include <nc_event.h>

#ifdef NC_HAVE_KQUEUE
#include <sys/event.h>

NcEventBase::NcEventBase(int nevent)
{
    int status, kq;
    struct kevent *change, *event;

    kq = kqueue();
    if (kq < 0) 
    {
        exit(0);
    }

    change = (struct kevent*)calloc(nevent, sizeof(struct kevent));
    if (event == NULL) 
    {
        status = ::close(kq);
        if (status < 0) 
        {
            LOG_ERROR("calloc error!!!");
            exit(0);
        }
    }

    event = (struct kevent*)calloc(nevent, sizeof(struct kevent));
    if (event == NULL) 
    {
        status = ::close(kq);
        if (status < 0) 
        {
            LOG_ERROR("calloc error!!!");
            exit(0);
        }
    }

    m_event_ = (struct _event*)malloc(sizeof(struct _event));
    if (m_event_ == NULL) 
    {
        ::free(change);
        ::free(event);
        status = ::close(kq);
        if (status < 0) 
        {
            LOG_ERROR("malloc error!!!");
            exit(0);
        }
    }

    m_event_->kq = kq;
    m_event_->change = change;
    m_event_->nchange = 0;
    m_event_->event = event;
    m_event_->nevent = nevent;
    m_event_->nreturned = 0;
    m_event_->nprocessed = 0;
}

NcEventBase::~NcEventBase()
{
    int status;

    if (m_event_ == NULL) 
    {
        return ;
    }

    ::free(m_event_->change);
    ::free(m_event_->event);

    status = ::close(m_event_->kq);
    if (status < 0) 
    {
        LOG_ERROR("close error, kq:%d", m_event_->kq);
    }

    m_event_->kq = -1;
    ::free(m_event_);
}

int NcEventBase::addInput(NcConnBase *c)
{
    LOG_DEBUG("addInput c : %p, active : %d", c, c->m_recv_active_);

    struct kevent *event;

    if (c->m_recv_active_) 
    {
        return 0;
    }

    event = &(m_event_->change[(m_event_->nchange)++]);
    EV_SET(event, c->m_sd_, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, c);
    c->m_recv_active_ = 1;

    return 0;
}

int NcEventBase::delInput(NcConnBase *c)
{
    struct kevent *event;
    
    if (!c->m_recv_active_) 
    {
        return 0;
    }

    event = &(m_event_->change[(m_event_->nchange)++]);
    EV_SET(event, c->m_sd_, EVFILT_READ, EV_DELETE, 0, 0, c);
    c->m_recv_active_ = 0;

    return 0;
}

int NcEventBase::addOutput(NcConnBase *c)
{
    LOG_DEBUG("addOutput c : %p, active : %d", c, c->m_send_active_);

    struct kevent *event;
    
    if (c->m_send_active_) 
    {
        return 0;
    }

    event = &(m_event_->change[(m_event_->nchange)++]);
    EV_SET(event, c->m_sd_, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, c);
    c->m_send_active_ = 1;

    return 0;
}

int NcEventBase::delOutput(NcConnBase *c)
{
    struct kevent *event;
    
    if (!c->m_send_active_) 
    {
        return 0;
    }

    event = &(m_event_->change[(m_event_->nchange)++]);
    EV_SET(event, c->m_sd_, EVFILT_WRITE, EV_DELETE, 0, 0, c);
    c->m_send_active_ = 0;

    return 0;
}

int NcEventBase::addConn(NcConnBase *c, uint32_t events)
{
    if (events & EVENT_READ)
    {
        addInput(c);
    }
    
    if (events & EVENT_WRITE)
    {
        addOutput(c);
    }

    return 0;
}

int NcEventBase::delConn(NcConnBase *c)
{
    delOutput(c);
    delInput(c);
    /*
     * Now, eliminate pending events for c->sd (there should be at most one
     * other event). This is important because we will close c->sd and free
     * c when we return.
     */
    for (int i = m_event_->nprocessed + 1; i < m_event_->nreturned; i++) 
    {
        struct kevent *ev = &m_event_->event[i];
        if (ev->ident == (uintptr_t)c->m_sd_) 
        {
            ev->flags = 0;
            ev->filter = 0;
            break;
        }
    }

    return 0;
}

int NcEventBase::wait(int timeout)
{
    int kq = m_event_->kq;
    struct timespec ts, *tsp;

    /* kevent should block indefinitely if timeout < 0 */
    if (timeout < 0) 
    {
        tsp = NULL;
    } 
    else 
    {
        tsp = &ts;
        tsp->tv_sec = timeout / 1000LL;
        tsp->tv_nsec = (timeout % 1000LL) * 1000000LL;
    }

    for (;;) 
    {
        /*
         * kevent() is used both to register new events with kqueue, and to
         * retrieve any pending events. Changes that should be applied to the
         * kqueue are given in the change[] and any returned events are placed
         * in event[], up to the maximum sized allowed by nevent. The number
         * of entries actually placed in event[] is returned by the kevent()
         * call and saved in nreturned.
         *
         * Events are registered with the system by the application via a
         * struct kevent, and an event is uniquely identified with the system
         * by a (kq, ident, filter) tuple. This means that there can be only
         * one (ident, filter) pair for a given kqueue.
         */
        m_event_->nreturned = kevent(kq, m_event_->change, 
            m_event_->nchange, m_event_->event, m_event_->nevent, tsp);
        m_event_->nchange = 0;
        if (m_event_->nreturned > 0) 
        {
            for (m_event_->nprocessed = 0; 
                m_event_->nprocessed < m_event_->nreturned;
                m_event_->nprocessed++) 
            {
                struct kevent *ev = &m_event_->event[m_event_->nprocessed];
                uint32_t events = 0;

                LOG_DEBUG("kevent %04" PRIX32 " with filter %d "
                    "triggered on sd %d", ev->flags, ev->filter,
                    ev->ident);

                /*
                 * If an error occurs while processing an element of the
                 * change[] and there is enough room in the event[], then the
                 * event event will be placed in the eventlist with EV_ERROR
                 * set in flags and the system error(errno) in data.
                 */
                if (ev->flags & EV_ERROR) 
                {
                   /*
                    * Error messages that can happen, when a delete fails.
                    *   EBADF happens when the file descriptor has been closed
                    *   ENOENT when the file descriptor was closed and then
                    *   reopened.
                    *   EINVAL for some reasons not understood; EINVAL
                    *   should not be returned ever; but FreeBSD does :-\
                    * An error is also indicated when a callback deletes an
                    * event we are still processing. In that case the data
                    * field is set to ENOENT.
                    */
                    if (ev->data == EBADF || ev->data == EINVAL ||
                        ev->data == ENOENT || ev->data == EINTR) 
                    {
                        continue;
                    }
                    events |= EVENT_ERR;
                }

                if (ev->filter == EVFILT_READ) 
                {
                    events |= EVENT_READ;
                }

                if (ev->filter == EVFILT_WRITE) 
                {
                    events |= EVENT_WRITE;
                }

                NcConnBase *c = (NcConnBase*)(ev->udata);
                LOG_DEBUG("ev->filter : %d, events : %d, c : %p", 
                    ev->filter, events, c);
                if (NULL != c) 
                {
                    c->callback(events);
                }
            }

            return m_event_->nreturned;
        }

        if (m_event_->nreturned == 0) 
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