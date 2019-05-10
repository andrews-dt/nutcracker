#include <nc_log.h>
#include <nc_util.h>

NcLogger::NcLogger()
{
    m_fd_ = 1;
    m_level_ = LLOG_PVERB;
}

NcLogger::~NcLogger()
{
    if (m_fd_ > 0 && m_fd_ != STDERR_FILENO) 
    {
        close(m_fd_);
        free(m_name_);
    }
}

int NcLogger::init(int level, char *name)
{
    m_level_ = MAX(LLOG_EMERG, MIN(level, LLOG_PVERB));
    m_name_ = NULL;
    if (name == NULL || !strlen(name)) 
    {
        m_fd_ = STDERR_FILENO;
    } 
    else 
    {
        m_name_ = strdup(name);
        m_fd_ = open(name, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (m_fd_ < 0) 
        {
            return -1;
        }
    }

    return 0;
}

void NcLogger::reopen()
{
    if (m_fd_ != STDERR_FILENO) 
    {
        close(m_fd_);
        m_fd_ = open(m_name_, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (m_fd_ < 0) 
        {
            // 打开错误
        }
    }
}

void NcLogger::setLevel(int level)
{
    m_level_ = MAX(LLOG_EMERG, MIN(level, LLOG_PVERB));
}

void NcLogger::stacktrace(void)
{
    if (m_fd_ < 0) 
    {
        return ;
    }
    // TODO : 跟踪数据
}

int NcLogger::logAble(int level)
{
    if (level > m_level_) 
    {
        return 0;
    }

    return 1;
}

int NcLogger::stringIndexOf(const char *s, char c)
{
    int i = -1;
    if (s != NULL && *s != '\0')
    {
        i = 0;
        while (*s != '\0' && *s != c)
        {
            s++;
            i++;
        }
    }

    return i;
}

int NcLogger::stringLastOf(const char *s, char c)
{
    int i = -1;
    if (s != NULL && *s != '\0')
    {
        int last = 0;
        i = 0;
        while (*s != '\0')
        {
            if (*s == c)
            {
                if (i >= last)
                {
                    last = i;
                }
            }
            s++;
            i++;
        }

        i = last;
    }

    return i;
}

void NcLogger::_log(const char *file, int line, int panic, const char *fmt, ...)
{
    static char buf[LOG_MAX_LEN];
    
    int len, size, errno_save;
    va_list args;
    ssize_t n;
    struct timeval tv;

    if (m_fd_ < 0) 
    {
        return;
    }

    errno_save = errno;
    len = 0;            /* length of output buffer */
    size = LOG_MAX_LEN; /* size of output buffer */

    gettimeofday(&tv, NULL);
    buf[len++] = '[';
    len += nc_strftime(buf + len, size - len, "%Y-%m-%d %H:%M:%S.", localtime(&tv.tv_sec));
    len += nc_scnprintf(buf + len, size - len, "%03ld", tv.tv_usec/1000);
    
    // 修改文件的temp
    char filetemp[256];
    int filetemp_len = stringLastOf(file, '/') + 1;
    strncpy(filetemp, file + filetemp_len, strlen(file) - filetemp_len + 1);
    len += nc_scnprintf(buf + len, size - len, "] [%s:%d] ", filetemp, line);

    va_start(args, fmt);
    len += nc_vscnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    n = nc_write(m_fd_, buf, len);
    if (n < 0) 
    {
        m_nerror_++;
    }

    errno = errno_save;

    if (panic) 
    {
        abort();
    }
}

void NcLogger::_loga(const char *fmt, ...)
{
    static char buf[LOG_MAX_LEN];
    
    int len, size, errno_save;
    va_list args;
    ssize_t n;

    if (m_fd_ < 0) 
    {
        return;
    }

    errno_save = errno;
    len = 0;            /* length of output buffer */
    size = LOG_MAX_LEN; /* size of output buffer */

    va_start(args, fmt);
    len += nc_vscnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    n = nc_write(m_fd_, buf, len);
    if (n < 0) 
    {
        m_nerror_++;
    }

    errno = errno_save;
}