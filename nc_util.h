#ifndef _NC_UTIL_H_
#define _NC_UTIL_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nc_string.h>
#include <nc_log.h>

#if (IOV_MAX > 128)
#define NC_IOV_MAX 128
#else
#define NC_IOV_MAX IOV_MAX
#endif

// 全局的NC的状态码
#define RESERVED_FDS 32
#define NC_OK        0
#define NC_ERROR    -1
#define NC_EAGAIN   -2
#define NC_ENOMEM   -3

typedef int rstatus_t;  /* return type */
typedef int err_t;      /* error type */

#define LF                  (uint8_t) 10
#define CR                  (uint8_t) 13
#define CRLF                "\x0d\x0a"
#define CRLF_LEN            (sizeof("\x0d\x0a") - 1)

#define NELEMS(a)           ((sizeof(a)) / sizeof((a)[0]))

#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))

#define SQUARE(d)           ((d) * (d))
#define VAR(s, s2, n)       (((n) < 2) ? 0.0 : ((s2) - SQUARE(s)/(n)) / ((n) - 1))
#define STDDEV(s, s2, n)    (((n) < 2) ? 0.0 : sqrt(VAR((s), (s2), (n))))

#define NC_INET4_ADDRSTRLEN (sizeof("255.255.255.255") - 1)
#define NC_INET6_ADDRSTRLEN (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)
#define NC_INET_ADDRSTRLEN  MAX(NC_INET4_ADDRSTRLEN, NC_INET6_ADDRSTRLEN)
#define NC_UNIX_ADDRSTRLEN  (sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

#define NC_MAXHOSTNAMELEN   256

/*
 * Length of 1 byte, 2 bytes, 4 bytes, 8 bytes and largest integral
 * type (uintmax_t) in ascii, including the null terminator '\0'
 *
 * From stdint.h, we have:
 * # define UINT8_MAX	(255)
 * # define UINT16_MAX	(65535)
 * # define UINT32_MAX	(4294967295U)
 * # define UINT64_MAX	(__UINT64_C(18446744073709551615))
 */
#define NC_UINT8_MAXLEN     (3 + 1)
#define NC_UINT16_MAXLEN    (5 + 1)
#define NC_UINT32_MAXLEN    (10 + 1)
#define NC_UINT64_MAXLEN    (20 + 1)
#define NC_UINTMAX_MAXLEN   NC_UINT64_MAXLEN

/*
 * Make data 'd' or pointer 'p', n-byte aligned, where n is a power of 2
 * of 2.
 */
#define NC_ALIGNMENT        sizeof(unsigned long) /* platform word */
#define NC_ALIGN(d, n)      (((d) + (n - 1)) & ~(n - 1))
#define NC_ALIGN_PTR(p, n)  (void *) (((uintptr_t) (p) + ((uintptr_t) n - 1)) & ~((uintptr_t) n - 1))

/*
 * Wrapper to workaround well known, safe, implicit type conversion when
 * invoking system calls.
 */
#define nc_gethostname(_name, _len) gethostname((char *)_name, (size_t)_len)

#define nc_atoi(_line, _n)          _nc_atoi((uint8_t *)_line, (size_t)_n)

#define nc_alloc(_s)                _nc_alloc((size_t)(_s), __FILE__, __LINE__)

#define nc_zalloc(_s)               _nc_zalloc((size_t)(_s), __FILE__, __LINE__)

#define nc_calloc(_n, _s)           _nc_calloc((size_t)(_n), (size_t)(_s), __FILE__, __LINE__)

#define nc_realloc(_p, _s)          _nc_realloc(_p, (size_t)(_s), __FILE__, __LINE__)

#define nc_free(_p) do                      \
    {                                       \
        _nc_free(_p, __FILE__, __LINE__);   \
        (_p) = NULL;                        \
    } while (0)

#define nc_delete(_p) do                    \
    {                                       \
        if (_p != NULL) delete (_p);        \
    } while (0)

#define nc_delete_arr(_p) do                \
    {                                       \
        if (_p != NULL) delete [] (_p);     \
    } while (0)

#define nc_sendn(_s, _b, _n)    _nc_sendn(_s, _b, (size_t)(_n))

#define nc_recvn(_s, _b, _n)    _nc_recvn(_s, _b, (size_t)(_n))

#define nc_read(_d, _b, _n)     read(_d, _b, (size_t)(_n))

#define nc_readv(_d, _b, _n)    readv(_d, _b, (int)(_n))

#define nc_write(_d, _b, _n)    write(_d, _b, (size_t)(_n))

#define nc_writev(_d, _b, _n)   writev(_d, _b, (int)(_n))

// 系统函数
extern "C"
{
    int _nc_atoi(uint8_t *line, size_t n);

    void* _nc_alloc(size_t size, const char *name, int line);

    void* _nc_zalloc(size_t size, const char *name, int line);

    void* _nc_calloc(size_t nmemb, size_t size, const char *name, int line);

    void* _nc_realloc(void *ptr, size_t size, const char *name, int line);

    void _nc_free(void *ptr, const char *name, int line);

    ssize_t _nc_sendn(int sd, const void *vptr, size_t n);

    ssize_t _nc_recvn(int sd, void* vptr, size_t n);

    int _scnprintf(char *buf, size_t size, const char *fmt, ...);

    int _vscnprintf(char *buf, size_t size, const char *fmt, va_list args);
}

struct sockinfo 
{
    int       family;              /* socket address family */
    socklen_t addrlen;             /* socket address length */
    union 
    {
        struct sockaddr_in  in;    /* ipv4 socket address */
        struct sockaddr_in6 in6;   /* ipv6 socket address */
        struct sockaddr_un  un;    /* unix domain address */
    } addr;
};

class NcUtil
{
public:
    inline static bool ncValidPort(int n)
    {
        if (n < 1 || n > UINT16_MAX) 
        {
            return false;
        }

        return true;
    }

    inline static int ncSetBlocking(int sd)
    {
        int flags;

        flags = ::fcntl(sd, F_GETFL, 0);
        if (flags < 0) 
        {
            return flags;
        }

        return ::fcntl(sd, F_SETFL, flags & ~O_NONBLOCK);
    }

    inline static int ncSetNonBlocking(int sd)
    {
        int flags;

        flags = ::fcntl(sd, F_GETFL, 0);
        if (flags < 0) 
        {
            return flags;
        }

        return ::fcntl(sd, F_SETFL, flags | O_NONBLOCK);
    }

    inline static int ncSetReuseAddr(int sd)
    {
        int reuse;
        socklen_t len;

        reuse = 1;
        len = sizeof(reuse);

        return ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, len);
    }

    inline static int ncSetTcpNodelay(int sd)
    {
        int nodelay;
        socklen_t len;

        nodelay = 1;
        len = sizeof(nodelay);

        return ::setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
    }

    inline static int ncSetLinger(int sd, int timeout)
    {
        struct linger linger;
        socklen_t len;

        linger.l_onoff = 1;
        linger.l_linger = timeout;

        len = sizeof(linger);

        return ::setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
    }

    inline static int ncSetSndBuf(int sd, int size)
    {
        socklen_t len;
        len = sizeof(size);

        return ::setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, len);
    }

    inline static int ncSetRcvBuf(int sd, int size)
    {
        socklen_t len;

        len = sizeof(size);

        return ::setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, len);
    }

    inline static int ncSetTcpKeepalive(int sd)
    {
        int val = 1;
        return ::setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
    }

    inline static int ncGetSoError(int sd)
    {
        int status, err;
        socklen_t len;

        err = 0;
        len = sizeof(err);

        status = ::getsockopt(sd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (status == 0) 
        {
            errno = err;
        }

        return status;
    }

    inline static int ncGetSndBuf(int sd)
    {
        int status, size;
        socklen_t len;

        size = 0;
        len = sizeof(size);

        status = ::getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, &len);
        if (status < 0) 
        {
            return status;
        }

        return size;
    }

    inline static int ncGetRcvBuf(int sd)
    {
        int status, size;
        socklen_t len;

        size = 0;
        len = sizeof(size);

        status = ::getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &len);
        if (status < 0) 
        {
            return status;
        }

        return size;
    }

    inline static int64_t ncUsecNow(void)
    {
        struct timeval now;
        int64_t usec;
        int status;

        status = ::gettimeofday(&now, NULL);
        if (status < 0) 
        {
            return -1;
        }

        usec = (int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec;

        return usec;
    }

    inline static int64_t ncMsecNow(void)
    {
        return ncUsecNow() / 1000LL;
    }

    inline static int ncResolve(NcString *name, int port, struct sockinfo *si)
    {
        if (name == NULL || name->length() <= 0)
        {
            return -1;
        }

        if ((name->c_str())[0] == '/') // unix
        {
            struct sockaddr_un *un;

            if (name->length() >= NC_UNIX_ADDRSTRLEN) 
            {
                return -1;
            }

            un = &si->addr.un;

            un->sun_family = AF_UNIX;
            nc_memcpy(un->sun_path, name->c_str(), name->length());
            un->sun_path[name->length()] = '\0';

            si->family = AF_UNIX;
            si->addrlen = sizeof(*un);

            return 0;
        }
        else
        {
            int status;
            struct addrinfo *ai, *cai; /* head and current addrinfo */
            struct addrinfo hints;
            char *node, service[NC_UINTMAX_MAXLEN];
            bool found;

            memset(&hints, 0, sizeof(hints));
            hints.ai_flags = AI_NUMERICSERV;
            hints.ai_family = AF_UNSPEC;     /* AF_INET or AF_INET6 */
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = 0;
            hints.ai_addrlen = 0;
            hints.ai_addr = NULL;
            hints.ai_canonname = NULL;

            if (name != NULL) 
            {
                node = (char *)name->c_str();
            } 
            else 
            {
                /*
                * If AI_PASSIVE flag is specified in hints.ai_flags, and node is
                * NULL, then the returned socket addresses will be suitable for
                * bind(2)ing a socket that will accept(2) connections. The returned
                * socket address will contain the wildcard IP address.
                */
                node = NULL;
                hints.ai_flags |= AI_PASSIVE;
            }
            nc_snprintf(service, NC_UINTMAX_MAXLEN, "%d", port);

            /*
            * getaddrinfo() returns zero on success or one of the error codes listed
            * in gai_strerror(3) if an error occurs
            */
            status = getaddrinfo(node, service, &hints, &ai);
            if (status != 0) 
            {
                return -1;
            }

            /*
            * getaddrinfo() can return a linked list of more than one addrinfo,
            * since we requested for both AF_INET and AF_INET6 addresses and the
            * host itself can be multi-homed. Since we don't care whether we are
            * using ipv4 or ipv6, we just use the first address from this collection
            * in the order in which it was returned.
            *
            * The sorting function used within getaddrinfo() is defined in RFC 3484;
            * the order can be tweaked for a particular system by editing
            * /etc/gai.conf
            */
            for (cai = ai, found = false; cai != NULL; cai = cai->ai_next) 
            {
                si->family = cai->ai_family;
                si->addrlen = cai->ai_addrlen;
                nc_memcpy(&si->addr, cai->ai_addr, si->addrlen);
                found = true;
                break;
            }

            ::freeaddrinfo(ai);
            return !found ? -1 : 0;
        }
    }

    inline static char* ncUnresolveAddr(struct sockaddr *addr, socklen_t addrlen)
    {
        static char unresolve[NI_MAXHOST + NI_MAXSERV];
        static char host[NI_MAXHOST], service[NI_MAXSERV];

        int status = ::getnameinfo(addr, addrlen, host, sizeof(host),
            service, sizeof(service),
            NI_NUMERICHOST | NI_NUMERICSERV);
        if (status < 0) 
        {
            return (char *)"unknown";
        }

        nc_snprintf(unresolve, sizeof(unresolve), "%s:%s", host, service);

        return unresolve;
    }

    // 获取对端地址
    inline static char* ncUnResolvePeerDesc(int sd)
    {
        static struct sockinfo si;

        memset(&si, 0, sizeof(si));
        struct sockaddr* addr = (struct sockaddr *)&si.addr;
        socklen_t addrlen = sizeof(si.addr);

        int status = ::getpeername(sd, addr, &addrlen);
        if (status < 0) 
        {
            return (char *)"unknown";
        }

        return ncUnresolveAddr(addr, addrlen);
    }

    inline static char* ncUnResolveDesc(int sd)
    {
        static struct sockinfo si;

        memset(&si, 0, sizeof(si));
        struct sockaddr* addr = (struct sockaddr *)&si.addr;
        socklen_t addrlen = sizeof(si.addr);

        int status = ::getsockname(sd, addr, &addrlen);
        if (status < 0) 
        {
            return (char *)"unknown";
        }

        return ncUnresolveAddr(addr, addrlen);
    }

    inline static uint64_t uniqNextId()
    {
        static uint64_t i = 1;
        return i++;
    }

    static rstatus_t ncDaemonize(int dump_core);

    static rstatus_t ncCreatePidfile(const char *name, int _pid);

    static void ncRemovePidfile(const char *name);

    // 全局的数据
    /* total # connections counter from start */
    inline static uint64_t ncTotalConn(int incr = 0)
    {
        static uint64_t t = 0;
        t += incr;
        return t; 
    }

    /* current # connections */
    inline static uint32_t ncCurConn(int incr = 0)
    {
        static uint32_t t = 0;
        t += incr;
        return t; 
    }

    /* current # client connections */
    inline static uint32_t ncCurCConn(int incr = 0)
    {
        static uint32_t t = 0;
        t += incr;
        return t; 
    }

    /* 设置分配内存的chunk_size */
    inline static uint32_t ncMbufChunkSize(uint32_t n = 0)
    {
        static uint32_t size = 10240;
        if (n > 0)
        {
            size = n;
        }
        
        return size;
    }
};

#endif
