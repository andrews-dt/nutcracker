#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include <nc_core.h>
#include <nc_signal.h>
#include <nc_mbuf.h>

#define NC_CONF_PATH        "../conf/nutcracker.yml"
#define NC_VERSION_STRING   "twemproxy v1.0"

#define NC_LOG_DEFAULT      LLOG_VVVERB
#define NC_LOG_MIN          LLOG_EMERG
#define NC_LOG_MAX          LLOG_PVERB
#define NC_LOG_PATH         "/dev/stdout"

#define STATS_ADDR          "0.0.0.0"
#define STATS_PORT          22222
#define STATS_INTERVAL      (30 * 1000) /* in msec */

#define NC_STATS_PORT       STATS_PORT
#define NC_STATS_ADDR       STATS_ADDR
#define NC_STATS_INTERVAL   STATS_INTERVAL

#define NC_PID_FILE         NULL

#define NC_MBUF_SIZE        MBUF_SIZE
#define NC_MBUF_MIN_SIZE    MBUF_MIN_SIZE
#define NC_MBUF_MAX_SIZE    MBUF_MAX_SIZE

static int s_show_help;
static int s_show_version;
static int s_test_conf;
static int s_daemonize;
static int s_describe_stats;

static struct option s_long_options[] = {
    { "help",           no_argument,        NULL,   'h' },
    { "version",        no_argument,        NULL,   'V' },
    { "test-conf",      no_argument,        NULL,   't' },
    { "daemonize",      no_argument,        NULL,   'd' },
    { "describe-stats", no_argument,        NULL,   'D' },
    { "verbose",        required_argument,  NULL,   'v' },
    { "output",         required_argument,  NULL,   'o' },
    { "conf-file",      required_argument,  NULL,   'c' },
    { "stats-port",     required_argument,  NULL,   's' },
    { "stats-interval", required_argument,  NULL,   'i' },
    { "stats-addr",     required_argument,  NULL,   'a' },
    { "pid-file",       required_argument,  NULL,   'p' },
    { "mbuf-size",      required_argument,  NULL,   'm' },
    { NULL,             0,                  NULL,    0  },
};

static char s_short_options[] = "hVtdDv:o:c:s:i:a:p:m:";

static void ncShowUsage(void)
{
    LOGA(
        "Usage: nutcracker [-?hVdDt] [-v verbosity level] [-o output file]" CRLF
        "                  [-c conf file] [-s stats port] [-a stats addr]" CRLF
        "                  [-i stats interval] [-p pid file] [-m mbuf size]" CRLF
        "");
    LOGA(
        "Options:" CRLF
        "  -h, --help             : this help" CRLF
        "  -V, --version          : show version and exit" CRLF
        "  -t, --test-conf        : test configuration for syntax errors and exit" CRLF
        "  -d, --daemonize        : run as a daemon" CRLF
        "  -D, --describe-stats   : print stats description and exit");
    LOGA(
        "  -v, --verbose=N        : set logging level (default: %d, min: %d, max: %d)" CRLF
        "  -o, --output=S         : set logging file (default: %s)" CRLF
        "  -c, --conf-file=S      : set configuration file (default: %s)" CRLF
        "  -s, --stats-port=N     : set stats monitoring port (default: %d)" CRLF
        "  -a, --stats-addr=S     : set stats monitoring ip (default: %s)" CRLF
        "  -i, --stats-interval=N : set stats aggregation interval in msec (default: %d msec)" CRLF
        "  -p, --pid-file=S       : set pid file (default: %s)" CRLF
        "  -m, --mbuf-size=N      : set size of mbuf chunk in bytes (default: %d bytes)" CRLF
        "",
        NC_LOG_DEFAULT, NC_LOG_MIN, NC_LOG_MAX,
        NC_LOG_PATH != NULL ? NC_LOG_PATH : "stderr",
        NC_CONF_PATH,
        NC_STATS_PORT, NC_STATS_ADDR, NC_STATS_INTERVAL,
        NC_PID_FILE != NULL ? NC_PID_FILE : "off",
        NC_MBUF_SIZE);
}

static rstatus_t ncGetOptions(int argc, char **argv, NcInstance *nci)
{
    int c, value;

    for (;;) 
    {
        c = getopt_long(argc, argv, s_short_options, s_long_options, NULL);
        if (c == -1) 
        {
            break;
        }

        switch (c) 
        {
        case 'h':
            s_show_version = 1;
            s_show_help = 1;
            break;
        case 'V':
            s_show_version = 1;
            break;
        case 't':
            s_test_conf = 1;
            break;
        case 'd':
            s_daemonize = 1;
            break;
        case 'D':
            s_describe_stats = 1;
            s_show_version = 1;
            break;
        case 'v':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0) 
            {
                LOG_ERROR("nutcracker: option -v requires a number");
                return NC_ERROR;
            }
            nci->log_level = value;
            break;
        case 'o':
            nci->log_filename = optarg;
            break;
        case 'c':
            nci->conf_filename = optarg;
            break;
        case 's':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0) 
            {
                LOG_ERROR("nutcracker: option -s requires a number");
                return NC_ERROR;
            }
            if (!NcUtil::nc_valid_port(value)) 
            {
                LOG_ERROR("nutcracker: option -s value %d is not a valid port", value);
                return NC_ERROR;
            }
            nci->stats_port = (uint16_t)value;
            break;
        case 'i':
            value = nc_atoi(optarg, strlen(optarg));
            if (value < 0) 
            {
                LOG_ERROR("nutcracker: option -i requires a number");
                return NC_ERROR;
            }
            nci->stats_interval = value;
            break;
        case 'a':
            nci->stats_addr = optarg;
            break;
        case 'p':
            nci->pid_filename = optarg;
            break;
        case 'm':
            value = nc_atoi(optarg, strlen(optarg));
            if (value <= 0) 
            {
                LOG_ERROR("nutcracker: option -m requires a non-zero number");
                return NC_ERROR;
            }
            if (value < NC_MBUF_MIN_SIZE || value > NC_MBUF_MAX_SIZE) 
            {
                LOG_ERROR("nutcracker: mbuf chunk size must be between %zu and"
                           " %zu bytes", NC_MBUF_MIN_SIZE, NC_MBUF_MAX_SIZE);
                return NC_ERROR;
            }
            nci->mbuf_chunk_size = (size_t)value;
            break;
        case '?':
            switch (optopt) {
            case 'o':
            case 'c':
            case 'p':
                LOG_ERROR("nutcracker: option -%c requires a file name",
                           optopt);
                break;
            case 'm':
            case 'v':
            case 's':
            case 'i':
                LOG_ERROR("nutcracker: option -%c requires a number", optopt);
                break;
            case 'a':
                LOG_ERROR("nutcracker: option -%c requires a string", optopt);
                break;
            default:
                LOG_ERROR("nutcracker: invalid option -- '%c'", optopt);
                break;
            }
            return NC_ERROR;
        default:
            LOG_ERROR("nutcracker: invalid option -- '%c'", optopt);
            return NC_ERROR;
        }
    }

    return NC_OK;
}

// 设置默认的值
static void ncSetDefaultOptions(NcInstance *nci)
{
    if (nci == NULL)
    {
        LOG_DEBUG("NcInstance is NULL");
        return ;
    }

    nci->log_level = NC_LOG_DEFAULT;
    nci->log_filename = NC_LOG_PATH;

    nci->conf_filename = NC_CONF_PATH;

    nci->stats_port = NC_STATS_PORT;
    nci->stats_addr = NC_STATS_ADDR;
    nci->stats_interval = NC_STATS_INTERVAL;

    nci->hostname[NC_MAXHOSTNAMELEN - 1] = '\0';
    int status = nc_gethostname(nci->hostname, NC_MAXHOSTNAMELEN);
    if (status < 0) 
    {
        LOG_WARN("gethostname failed, ignored: %s", strerror(errno));
        nc_snprintf(nci->hostname, NC_MAXHOSTNAMELEN, "unknown");
    }

    nci->mbuf_chunk_size = NC_MBUF_SIZE;

    nci->pid = (pid_t)-1;
    nci->pid_filename = NULL;
    nci->pidfile = 0;
}

static bool ncTestConf(NcInstance *nci)
{
    if (nci == NULL || nci->conf_filename == NULL)
    {
        return false;
    }

    rstatus_t status = (nci->conf).openFile(nci->conf_filename);
    if (status != NC_OK)
    {
        LOG_DEBUG("status : %d", status);
        return false;
    }
    status = (nci->conf).parseProcess();
    if (status != NC_OK)
    {
        LOG_DEBUG("status : %d", status);
        return false;
    }
    (nci->conf).dump();

    return true;
}

// 初始化
static rstatus_t ncInit(NcInstance *nci)
{
    // 日志格式化
    rstatus_t status;
    // 解析配置文件
    status = (nci->conf).openFile(nci->conf_filename);
    if (status != NC_OK)
    {
        LOG_DEBUG("status : %d", status);
        return NC_ERROR;
    }
    status = (nci->conf).parseProcess();
    if (status != NC_OK)
    {
        LOG_DEBUG("status : %d", status);
        return NC_ERROR;
    }

    // 初始化日志
    status = NcLogger::getInstance().init(nci->log_level, nci->log_filename);
    if (status != NC_OK) 
    {
        return status;
    }
    
    if (s_daemonize) 
    {
        status = NcUtil::ncDaemonize(1);
        if (status != NC_OK) 
        {
            return status;
        }
    }

    nci->pid = getpid();

    // 初始化信号
    nci->signal = new NcSignal();
    if (nci->signal == NULL)
    {
        LOG_ERROR("new signal error!!!");
        return NC_ERROR;
    }

    struct utsname name;
    status = uname(&name);
    if (status < 0) 
    {
        LOGA("nutcracker-%s started on pid %d", NC_VERSION_STRING, nci->pid);
    } 
    else 
    {
        LOGA("nutcracker-%s built for %s %s %s started on pid %d",
             NC_VERSION_STRING, name.sysname, name.release, name.machine,
             nci->pid);
    }
    LOGA("run, rabbit run / dig that hole, forget the sun / "
         "and when at last the work is done / don't sit down / "
         "it's time to dig another one");

    return NC_OK;
}

int main(int argc, char **argv)
{
    NcInstance nci;
    
    ncSetDefaultOptions(&nci);

    rstatus_t status = ncGetOptions(argc, argv, &nci);
    if (status != NC_OK) 
    {
        ncShowUsage();
        exit(1);
    }

    if (s_show_version) 
    {
        LOGA("This is nutcracker-%s" CRLF, NC_VERSION_STRING);
        if (s_show_help) 
        {
            ncShowUsage();
        }
        if (s_describe_stats) 
        {
            // TODO:显示当前状态信息
        }
        exit(0);
    }

    // 测试配置文件
    if (s_test_conf) 
    {
        if (!ncTestConf(&nci)) 
        {
            exit(1);
        }
        exit(0);
    }

    status = ncInit(&nci);
    if (status == NC_OK) 
    {
        LOG_DEBUG("pool size:%d", nci.conf.pool.size());
        for (int i = 0; i < nci.conf.pool.size(); i++)
        {
            NcContext *ctx = nci.createContext(nci.conf.pool[i]);
            if (ctx == NULL)
            {
                exit(1);
            }
            nci.ctx.push_back(ctx);
        }

        // 监听，循环处理
        for (;;) 
        {
            status = nci.loop();
            if (status != NC_OK) 
            {
                break;
            }
        }
    }
    
    LOGA("done, rabbit done");
    exit(1);
}