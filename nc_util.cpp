#include <nc_util.h>

extern "C" int _nc_atoi(uint8_t *line, size_t n)
{
    int value;

    if (n == 0) 
    {
        return -1;
    }

    for (value = 0; n--; line++) 
    {
        if (*line < '0' || *line > '9') 
        {
            return -1;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) 
    {
        return -1;
    }

    return value;
}

extern "C" void* _nc_alloc(size_t size, const char *name, int line)
{
    void* p;

    p = malloc(size);
    if (p == NULL) 
    {
        LOG_ERROR("malloc(%zu) failed @ %s:%d", size, name, line);
    }

    return p;
}

extern "C" void* _nc_zalloc(size_t size, const char *name, int line)
{
    void *p;

    p = _nc_alloc(size, name, line);
    if (p != NULL) 
    {
        memset(p, 0, size);
    }

    return p;
}

extern "C" void* _nc_calloc(size_t nmemb, size_t size, const char *name, int line)
{
    return _nc_zalloc(nmemb * size, name, line);
}

extern "C" void* _nc_realloc(void* ptr, size_t size, const char *name, int line)
{
    void* p;

    p = realloc(ptr, size);
    if (p == NULL) 
    {
        LOG_ERROR("realloc(%zu) failed @ %s:%d", size, name, line);
    } 
    else 
    {
        LOG_DEBUG("realloc(%zu) at %p @ %s:%d", size, p, name, line);
    }

    return p;
}

extern "C" void _nc_free(void* ptr, const char *name, int line)
{
    LOG_DEBUG("free(%p) @ %s:%d", ptr, name, line);
    free(ptr);
}

extern "C" int _vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int n;

    n = vsnprintf(buf, size, fmt, args);
    if (n <= 0) 
    {
        return 0;
    }

    if (n < (int) size) 
    {
        return n;
    }

    return (int)(size - 1);
}

extern "C" int _scnprintf(char* buf, size_t size, const char* fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = _vscnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}

extern "C" ssize_t _nc_sendn(int sd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t	nsend;
    const char* ptr;

    ptr = (const char*)vptr;
    nleft = n;
    while (nleft > 0) 
    {
        nsend = send(sd, ptr, nleft, 0);
        if (nsend < 0) 
        {
            if (errno == EINTR) 
            {
                continue;
            }
            return nsend;
        }

        if (nsend == 0) 
        {
            return -1;
        }

        nleft -= (size_t)nsend;
        ptr += nsend;
    }

    return (ssize_t)n;
}

extern "C" ssize_t _nc_recvn(int sd, void* vptr, size_t n)
{
	size_t nleft;
	ssize_t	nrecv;
	char* ptr;

	ptr = (char *)vptr;
	nleft = n;
	while (nleft > 0) 
    {
        nrecv = recv(sd, ptr, nleft, 0);
        if (nrecv < 0) 
        {
            if (errno == EINTR) 
            {
                continue;
            }
            return nrecv;
        }
        
        if (nrecv == 0) 
        {
            break;
        }

        nleft -= (size_t)nrecv;
        ptr += nrecv;
    }

    return (ssize_t)(n - nleft);
}

rstatus_t NcUtil::ncDaemonize(int dump_core)
{
    pid_t pid = fork();
    switch (pid) 
    {
    case -1:
        LOG_ERROR("fork() failed: %s", strerror(errno));
        return NC_ERROR;
    case 0:
        break;
    default:
        /* parent terminates */
        _exit(0);
    }

    /* 1st child continues and becomes the session leader */
    pid_t sid = setsid();
    if (sid < 0) 
    {
        LOG_ERROR("setsid() failed: %s", strerror(errno));
        return NC_ERROR;
    }

    if (signal(SIGHUP, SIG_IGN) == SIG_ERR) 
    {
        LOG_ERROR("signal(SIGHUP, SIG_IGN) failed: %s", strerror(errno));
        return NC_ERROR;
    }

    pid = fork();
    switch (pid) 
    {
    case -1:
        LOG_ERROR("fork() failed: %s", strerror(errno));
        return NC_ERROR;
    case 0:
        break;
    default:
        _exit(0);
    }

    /* 2nd child continues */

    rstatus_t status;
    /* change working directory */
    if (dump_core == 0) 
    {
        status = chdir("/");
        if (status < 0) 
        {
            LOG_ERROR("chdir(\"/\") failed: %s", strerror(errno));
            return NC_ERROR;
        }
    }

    /* clear file mode creation mask */
    umask(0);

    /* redirect stdin, stdout and stderr to "/dev/null" */
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) 
    {
        LOG_ERROR("open(\"/dev/null\") failed: %s", strerror(errno));
        return NC_ERROR;
    }

    status = dup2(fd, STDIN_FILENO);
    if (status < 0) 
    {
        LOG_ERROR("dup2(%d, STDIN) failed: %s", fd, strerror(errno));
        close(fd);
        return NC_ERROR;
    }

    status = dup2(fd, STDOUT_FILENO);
    if (status < 0) 
    {
        LOG_ERROR("dup2(%d, STDOUT) failed: %s", fd, strerror(errno));
        close(fd);
        return NC_ERROR;
    }

    status = dup2(fd, STDERR_FILENO);
    if (status < 0) 
    {
        LOG_ERROR("dup2(%d, STDERR) failed: %s", fd, strerror(errno));
        close(fd);
        return NC_ERROR;
    }

    if (fd > STDERR_FILENO) 
    {
        status = close(fd);
        if (status < 0) 
        {
            LOG_ERROR("close(%d) failed: %s", fd, strerror(errno));
            return NC_ERROR;
        }
    }

    return NC_OK;
}

rstatus_t NcUtil::ncCreatePidfile(const char *name, int _pid)
{
    char pid[NC_UINTMAX_MAXLEN];
    int fd, pid_len;
    ssize_t n;

    fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) 
    {
        LOG_ERROR("opening pid file '%s' failed: %s", name, strerror(errno));
        return NC_ERROR;
    }
    pid_len = nc_snprintf(pid, NC_UINTMAX_MAXLEN, "%d", pid);
    n = nc_write(fd, pid, pid_len);
    if (n < 0) 
    {
        LOG_ERROR("write to pid file '%s' failed: %s", name, strerror(errno));
        return NC_ERROR;
    }
    close(fd);

    return NC_OK;
}

void NcUtil::ncRemovePidfile(const char *name)
{
    int status = unlink(name);
    if (status < 0) 
    {
        LOG_ERROR("unlink of pid file '%s' failed, ignored: %s", name, strerror(errno));
    }
}