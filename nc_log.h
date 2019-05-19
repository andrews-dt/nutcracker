#ifndef _NC_LOG_H_
#define _NC_LOG_H_

#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>

#define LLOG_EMERG   0      /* system in unusable */
#define LLOG_ALERT   1      /* action must be taken immediately */
#define LLOG_CRIT    2      /* critical conditions */
#define LLOG_ERR     3      /* error conditions */
#define LLOG_WARN    4      /* warning conditions */
#define LLOG_NOTICE  5      /* normal but significant condition (default) */
#define LLOG_INFO    6      /* informational */
#define LLOG_DEBUG   7      /* debug messages */
#define LLOG_VERB    8      /* verbose messages */
#define LLOG_VVERB   9      /* verbose messages on crack */
#define LLOG_VVVERB  10     /* verbose messages on ganga */
#define LLOG_PVERB   11     /* periodic verbose messages on crack */

#define LOG_MAX_LEN  2046   /* max length of log message */

class NcLogger
{
public:
    NcLogger();
    
    ~NcLogger();

    int init(int level, char *filename);

    void reopen();

    void setLevel(int level);

    void stacktrace();

    int logAble(int level);

    void _log(const char *file, int line, int panic, const char *fmt, ...);

    void _loga(const char *fmt, ...);

    inline static NcLogger& getInstance()
    {
        static NcLogger logger;
        return logger;
    }

    // 日志特殊的indexOf和lastOf
    static int stringIndexOf(const char *s, char c);

    static int stringLastOf(const char *s, char c);

private:
    char *m_name_;
    int m_level_;
    int m_fd_;
    int m_nerror_;
};

#define LOG_ERROR(...) do                           \
    {                                                                           \
        if (NcLogger::getInstance().logAble(LLOG_ALERT) != 0) {                 \
            NcLogger::getInstance()._log(__FILE__, __LINE__, 0, __VA_ARGS__);   \
        }                                                                       \
    } while (0)

#define LOG_WARN(...) do                            \
    {                                                                           \
        if (NcLogger::getInstance().logAble(LLOG_WARN) != 0) {                  \
            NcLogger::getInstance()._log(__FILE__, __LINE__, 0, __VA_ARGS__);   \
        }                                                                       \
    } while (0)

#define LOG_PANIC(...) do                           \
    {                                                                           \
        if (NcLogger::getInstance().logAble(LLOG_EMERG) != 0) {                 \
            NcLogger::getInstance()._log(__FILE__, __LINE__, 1, __VA_ARGS__);   \
        }                                                                       \
    } while (0)

#define LOG_DEBUG(...) do                           \
    {                                                                           \
        if (NcLogger::getInstance().logAble(LLOG_VVVERB) != 0) {                \
            NcLogger::getInstance()._log(__FILE__, __LINE__, 0, __VA_ARGS__);   \
        }                                                                       \
    } while (0)

#define LOG_VERBOSE(...) do                         \
    {                                                                           \
        if (NcLogger::getInstance().logAble(LLOG_PVERB) != 0) {                 \
            NcLogger::getInstance()._log(__FILE__, __LINE__, 0, __VA_ARGS__);   \
        }                                                                       \
    } while (0)

#define LOGA(...) do                                \
    {                                                                       \
        if (NcLogger::getInstance().logAble(LLOG_PVERB) != 0) {             \
            NcLogger::getInstance()._loga(__VA_ARGS__);                     \
        }                                                                   \
    } while (0)

#define FUNCTION_INTO(cls)  LOG_DEBUG("\033[32mrun into %s:%s \033[0m", #cls, __FUNCTION__)

#define FUNCTION_OUT(cls)   LOG_DEBUG("\033[34mrun out %s:%s \033[0m", #cls, __FUNCTION__)

#define ASSERT(exp)         assert((exp))

#endif