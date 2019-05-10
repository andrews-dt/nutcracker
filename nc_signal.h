#ifndef _NC_SIGNAL_H_
#define _NC_SIGNAL_H_

#include <stdlib.h>
#include <signal.h>

#ifdef NO_TEST
#include <nc_core.h>
#endif

class NcSignal
{
public:
    NcSignal()
    {
        static int signals[] = {SIGUSR1, SIGUSR2, SIGTTIN,
            SIGTTOU, SIGHUP, SIGINT, SIGSEGV, SIGPIPE};

        for (uint32_t i = 0; i < sizeof(signals)/sizeof(signals[0]); i++) 
        {
            struct sigaction sa;

            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = NcSignal::signalHandler;
            if (signals[i] == SIGSEGV)
            {
                sa.sa_flags = SA_RESETHAND;
            }
            else
            {
                sa.sa_flags = 0;
            }

            sigemptyset(&sa.sa_mask);
            rstatus_t status = sigaction(signals[i], &sa, NULL);
            if (status < 0) 
            {
                LOG_ERROR("sigaction failed: %s", strerror(errno));
            }
        }
    }

    inline static void signalHandler(int signo)
    {
        LOG_ERROR("signal %d received", signo);
        exit(0); // 退出应用
    }
};

#endif
