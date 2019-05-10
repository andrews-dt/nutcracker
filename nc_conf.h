#ifndef _NC_CONF_H_
#define _NC_CONF_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <vector>
#include <yaml.h>

#include <nc_string.h>
#include <nc_util.h>

#define CONF_OK                 (void *) NULL
#define CONF_ERROR              (void *) "has an invalid value"

#define CONF_ROOT_DEPTH         1
#define CONF_MAX_DEPTH          CONF_ROOT_DEPTH + 1

#define CONF_DEFAULT_ARGS       3
#define CONF_DEFAULT_POOL       8
#define CONF_DEFAULT_SERVERS    8

#define CONF_UNSET_NUM          -1
#define CONF_UNSET_PTR          NULL
#define CONF_UNSET_HASH         (hash_type_t) -1
#define CONF_UNSET_DIST         (dist_type_t) -1

#define CONF_DEFAULT_HASH                    HASH_FNV1A_64
#define CONF_DEFAULT_DIST                    DIST_KETAMA
#define CONF_DEFAULT_TIMEOUT                 -1
#define CONF_DEFAULT_LISTEN_BACKLOG          512
#define CONF_DEFAULT_CLIENT_CONNECTIONS      0
#define CONF_DEFAULT_PRECONNECT              false
#define CONF_DEFAULT_AUTO_EJECT_HOSTS        false
#define CONF_DEFAULT_SERVER_RETRY_TIMEOUT    30 * 1000      /* in msec */
#define CONF_DEFAULT_SERVER_FAILURE_LIMIT    2
#define CONF_DEFAULT_SERVER_CONNECTIONS      1
#define CONF_DEFAULT_KETAMA_PORT             11211
#define CONF_DEFAULT_TCPKEEPALIVE            false

class NcConfListen 
{
public:
    NcConfListen() : valid(0)
    { }

    rstatus_t parse(NcString &value);

public:
    NcString        pname;   /* listen: as "hostname:port" */
    NcString        name;    /* hostname:port */
    int             port;    /* port */
    mode_t          perm;    /* socket permissions */
    struct sockinfo info;    /* listen socket info */
    unsigned        valid;   /* valid? */
};

class NcConfServer : public NcConfListen
{
public:
    NcConfServer() : weight(0)
    { }

    rstatus_t parse(NcString &value);

public:
    int             weight;     /* weight */
    NcString        addrstr;    /* hostname */
};

class NcConfPool 
{
public:
    NcConfPool()
    {
        timeout = CONF_DEFAULT_TIMEOUT;
        backlog = CONF_DEFAULT_LISTEN_BACKLOG;
        client_connections = CONF_DEFAULT_CLIENT_CONNECTIONS;
        tcpkeepalive = CONF_DEFAULT_TCPKEEPALIVE;
        preconnect = CONF_DEFAULT_PRECONNECT;
        auto_eject_hosts = CONF_DEFAULT_AUTO_EJECT_HOSTS;
        server_connections = CONF_DEFAULT_SERVER_CONNECTIONS;
        server_retry_timeout = CONF_DEFAULT_SERVER_RETRY_TIMEOUT;
        server_failure_limit = CONF_DEFAULT_SERVER_FAILURE_LIMIT;
    }

    ~NcConfPool()
    {
        for (uint32_t i = 0; i < server.size(); i++)
        {
            nc_delete(server[i]);
        }
    }

public:
    NcString        name;                  /* pool name (root node) */
    NcConfListen    listen;                /* listen: */
    NcString        hash_tag;              /* hash_tag: */
    int             distribution;
    int             timeout;               /* timeout: */
    int             backlog;               /* backlog: */
    int             client_connections;    /* client_connections: */
    int             tcpkeepalive;          /* tcpkeepalive: */
    int             preconnect;            /* preconnect: */
    int             auto_eject_hosts;      /* auto_eject_hosts: */
    int             server_connections;    /* server_connections: */
    int             server_retry_timeout;  /* server_retry_timeout: in msec */
    int             server_failure_limit;  /* server_failure_limit: */
    std::vector<NcConfServer*>       server;                /* servers: conf_server[] */
    unsigned        valid;                 /* valid? */
};

class NcConf
{
public:
    NcConf() : depth(0), seq(0), valid_parser(0), valid_event(0),
        valid_token(0), sound(0), parsed(0), valid(0)
    { }

    ~NcConf()
    {
        nc_free(fname);
    }

    rstatus_t openFile(const char *_fname);

    void dump();

    rstatus_t yamlInit();

    void yamlDeinit();

    rstatus_t yamlEventNext();

    void yamlEventDone();

    rstatus_t parseProcess();

    rstatus_t parseBegin();

    rstatus_t parseEnd();

    rstatus_t yamlPushScalar();

    void yamlPopScalar();

    static rstatus_t parseCore(NcConf *cf, NcConfPool *data);

    static rstatus_t handler(NcConf *cf, NcConfPool *data);
 
public:
    std::vector<NcConfPool*> pool;
    std::vector<NcString>    args;  // 参数
    char          *fname;           /* file name (ref in argv[]) */
    FILE          *fh;              /* file handle */
    uint32_t      depth;            /* parsed tree depth */
    yaml_parser_t parser;           /* yaml parser */
    yaml_event_t  event;            /* yaml event */
    yaml_token_t  token;            /* yaml token */
    unsigned      seq;              /* sequence? */
    unsigned      valid_parser;     /* valid parser? */
    unsigned      valid_event;      /* valid event? */
    unsigned      valid_token;      /* valid token? */
    unsigned      sound;            /* sound? */
    unsigned      parsed;           /* parsed? */
    unsigned      valid;            /* valid? */
};

#endif
