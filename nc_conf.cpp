#include <nc_conf.h>

rstatus_t NcConfListen::parse(NcString &value)
{
    FUNCTION_INTO(NcConfListen);

    LOG_DEBUG("value : %s, length : %d", value.c_str(), value.length());

    uint8_t *q, *start, *_perm, *_port;
    uint8_t *p, *_name;
    uint32_t _namelen, _permlen, _portlen;

    if ((value.c_str())[0] == '/') 
    {
        /* parse "socket_path permissions" from the end */
        p = (uint8_t*)value.c_str() + value.length() -1;
        start = (uint8_t*)value.c_str();
        q = nc_strrchr(p, start, ' ');
        if (q == NULL) 
        {
            /* no permissions field, so use defaults */
            _name = (uint8_t*)value.c_str();
            _namelen = value.length();
        } 
        else 
        {
            _perm = q + 1;
            _permlen = (uint32_t)(p - _perm + 1);

            p = q - 1;
            _name = start;
            _namelen = (uint32_t)(p - start + 1);

            errno = 0;
            perm = (mode_t)strtol((char *)_perm, NULL, 8);
            if (errno || perm > 0777) 
            {
                LOG_ERROR("has an invalid file permission in \"socket_path permission\" format string");
                return NC_ERROR;
            }
        }
    } 
    else 
    {
        /* parse "hostname:port" from the end */
        p = (uint8_t*)value.c_str() + value.length() -1;
        start = (uint8_t*)value.c_str();
        q = nc_strrchr(p, start, ':');
        LOG_DEBUG("start : %s, q : %p", start, q);
        if (q == NULL) 
        {
            LOG_ERROR("has an invalid \"hostname:port\" format string");
            return NC_ERROR;
        }
        _port = q + 1;
        _portlen = (uint32_t)(p - _port + 1);
        p = q - 1;

        _name = start;
        _namelen = (uint32_t)(p - start + 1);

        LOG_DEBUG("port : %s, portlen : %d", _port, _portlen);
        port = nc_atoi(_port, _portlen);
        if (port < 0 || !NcUtil::nc_valid_port(port)) 
        {
            LOG_ERROR("has an invalid port in \"hostname:port\" format string");
            return NC_ERROR;
        }
    }

    // 赋值操作
    pname = value;
    name = NcString(_name, _namelen);
    LOG_DEBUG("name : %s, port : %d", name.c_str(), port);
    rstatus_t status = NcUtil::nc_resolve(&name, port, &info);
    if (status != NC_OK) 
    {
        return NC_ERROR;
    }
    valid = 1;

    return NC_OK;
}

rstatus_t NcConfServer::parse(NcString &value)
{
    FUNCTION_INTO(NcConfServer);

    LOG_DEBUG("value : %s, length : %d", value.c_str(), value.length());

    uint8_t *q, *start, *_perm, *_port, *_weight;
    uint8_t *p, *_name;
    uint32_t _namelen, _permlen, _portlen, _weightlen;

    if ((value.c_str())[0] == '/') 
    {
        /* parse "socket_path permissions" from the end */
        p = (uint8_t*)value.c_str() + value.length() -1;
        start = (uint8_t*)value.c_str();
        q = nc_strrchr(p, start, ' ');
        if (q == NULL) 
        {
            LOG_ERROR("has an invalid \"hostname:port:weight\" format string");
            return NC_ERROR;
        }
        _weight = q + 1;
        _weightlen = (uint32_t)(p - _weight + 1);
        p = q - 1;

        q = nc_strrchr(p, start, ' ');
        if (q == NULL) 
        {
            /* no permissions field, so use defaults */
            _name = (uint8_t*)value.c_str();
            _namelen = value.length();
        } 
        else 
        {
            _perm = q + 1;
            _permlen = (uint32_t)(p - _perm + 1);

            p = q - 1;
            _name = start;
            _namelen = (uint32_t)(p - start + 1);

            errno = 0;
            perm = (mode_t)strtol((char *)_perm, NULL, 8);
            if (errno || perm > 0777) 
            {
                LOG_ERROR("has an invalid file permission in \"socket_path permission\" format string");
                return NC_ERROR;
            }
        }
    } 
    else 
    {
        /* parse "hostname:port" from the end */
        p = (uint8_t*)value.c_str() + value.length() -1;
        start = (uint8_t*)value.c_str();
        q = nc_strrchr(p, start, ':');
        LOG_DEBUG("start : %s, q : %p", start, q);
        if (q == NULL) 
        {
            LOG_ERROR("has an invalid \"hostname:port:weight\" format string");
            return NC_ERROR;
        }
        _weight = q + 1;
        _weightlen = (uint32_t)(p - _weight + 1);
        p = q - 1;

        q = nc_strrchr(p, start, ':');
        LOG_DEBUG("start : %s, q : %p", start, q);
        if (q == NULL) 
        {
            LOG_ERROR("has an invalid \"hostname:port\" format string");
            return NC_ERROR;
        }
        _port = q + 1;
        _portlen = (uint32_t)(p - _port + 1);

        p = q - 1;

        _name = start;
        _namelen = (uint32_t)(p - start + 1);

        LOG_DEBUG("port : %s, portlen : %d", _port, _portlen);
        port = nc_atoi(_port, _portlen);
        if (port < 0 || !NcUtil::nc_valid_port(port)) 
        {
            LOG_ERROR("has an invalid port in \"hostname:port\" format string");
            return NC_ERROR;
        }
    }

    weight = nc_atoi(_weight, _weightlen);
    LOG_DEBUG("weight : %d", weight);
    if (weight < 0 || weight > 0x7FFFFFFF)
    {
        LOG_ERROR("has an invalid weight format string");
        return NC_ERROR;
    }

    // 赋值操作
    pname = value;
    name = NcString(_name, _namelen);
    LOG_DEBUG("name : %s, port : %d", name.c_str(), port);
    rstatus_t status = NcUtil::nc_resolve(&name, port, &info);
    if (status != NC_OK) 
    {
        return NC_ERROR;
    }
    valid = 1;

    return NC_OK;
}

rstatus_t NcConf::openFile(const char *_fname)
{
    rstatus_t status;
    FILE *_fh = ::fopen(_fname, "r");
    if (_fh == NULL) 
    {
        LOG_ERROR("conf: failed to open configuration '%s': %s", _fname,
                  strerror(errno));
        return NC_ERROR;
    }

    fname = strdup(_fname);
    fh = _fh;

    LOG_DEBUG("opened conf '%s'", fname);

    return NC_OK;
}

void NcConf::dump()
{
    uint32_t npool = pool.size();
    LOG_DEBUG("%" PRIu32 " pools in configuration file '%s'", npool, fname);

    for (int i = 0; i < npool; i++) 
    {
        NcConfPool *cp = pool[i];
        if (cp == NULL)
        {
            LOG_ERROR("cp is NULL");
            continue;   
        }

        LOG_DEBUG("%.*s", cp->name.length(), cp->name.c_str());
        LOG_DEBUG("  listen: %.*s", cp->listen.pname.length(), cp->listen.pname.c_str());
        LOG_DEBUG("  timeout: %d", cp->timeout);
        LOG_DEBUG("  backlog: %d", cp->backlog);
        LOG_DEBUG("  hash_tag: \"%.*s\"", cp->hash_tag.length(),
                  cp->hash_tag.c_str());
        LOG_DEBUG("  distribution: %d", cp->distribution);
        LOG_DEBUG("  client_connections: %d",
                  cp->client_connections);
        LOG_DEBUG("  preconnect: %d", cp->preconnect);
        LOG_DEBUG("  auto_eject_hosts: %d", cp->auto_eject_hosts);
        LOG_DEBUG("  server_connections: %d", cp->server_connections);
        LOG_DEBUG("  server_retry_timeout: %d", cp->server_retry_timeout);
        LOG_DEBUG("  server_failure_limit: %d", cp->server_failure_limit);

        uint32_t nserver = cp->server.size();
        LOG_DEBUG("  servers: %" PRIu32 "", nserver);

        for (int j = 0; j < nserver; j++) 
        {
            NcConfServer *s = (cp->server)[j];
            LOG_DEBUG("    %.*s", s->pname.length(), s->pname.c_str());
        }
    }
}

rstatus_t NcConf::yamlInit()
{
    int rv = fseek(fh, 0L, SEEK_SET);
    if (rv < 0) 
    {
        LOG_ERROR("conf: failed to seek to the beginning of file '%s': %s",
            fname, strerror(errno));
        return NC_ERROR;
    }

    rv = yaml_parser_initialize(&parser);
    if (!rv) 
    {
        LOG_ERROR("conf: failed (err %d) to initialize yaml parser",
            parser.error);
        return NC_ERROR;
    }

    yaml_parser_set_input_file(&parser, fh);
    valid_parser = 1;

    return NC_OK;
}

void NcConf::yamlDeinit()
{
    if (valid_parser) 
    {
        yaml_parser_delete(&parser);
        valid_parser = 0;
    }
}

rstatus_t NcConf::yamlEventNext()
{
    int rv = yaml_parser_parse(&parser, &event);
    if (!rv) 
    {
        LOG_ERROR("conf: failed (err %d) to get next event", parser.error);
        return NC_ERROR;
    }

    valid_event = 1;

    return NC_OK;
}

void NcConf::yamlEventDone()
{
    FUNCTION_INTO(NcConf);

    if (valid_event) 
    {
        yaml_event_delete(&event);
        valid_event = 0;
    }
}

rstatus_t NcConf::parseProcess()
{
    rstatus_t status = parseBegin();
    if (status != NC_OK) 
    {
        return status;
    }

    status = parseCore(this, NULL);
    if (status != NC_OK) 
    {
        return status;
    }

    status = parseEnd();
    if (status != NC_OK) 
    {
        return status;
    }
    
    parsed = 1;
    
    return NC_OK;
}

rstatus_t NcConf::parseBegin()
{
    rstatus_t status = yamlInit();
    if (status != NC_OK) 
    {
        return status;
    }

    bool done = false;
    do {
        status = yamlEventNext();
        if (status != NC_OK) 
        {
            return status;
        }

        LOG_DEBUG("next begin event %d", event.type);

        switch (event.type) 
        {
        case YAML_STREAM_START_EVENT:
        case YAML_DOCUMENT_START_EVENT:
            break;

        case YAML_MAPPING_START_EVENT:
            depth++;
            done = true;
            break;

        default:
            break;
        }
        
        yamlEventDone();

    } while (!done);

    return NC_OK;
}

rstatus_t NcConf::parseEnd()
{
    bool done = false;
    do 
    {
        rstatus_t status = yamlEventNext();
        if (status != NC_OK) 
        {
            return status;
        }

        LOG_DEBUG("next end event %d", event.type);

        switch (event.type) 
        {
        case YAML_STREAM_END_EVENT:
            done = true;
            break;

        case YAML_DOCUMENT_END_EVENT:
            break;

        default:
            break;
        }

        yamlEventDone();

    } while (!done);

    yamlDeinit();

    return NC_OK;
}

rstatus_t NcConf::yamlPushScalar()
{
    uint8_t *scalar = event.data.scalar.value;
    uint32_t scalar_len = (uint32_t)event.data.scalar.length;
    if (scalar_len == 0) 
    {
        return NC_ERROR;
    }

    LOG_DEBUG("push '%.*s'", scalar_len, scalar);

    NcString value(scalar, scalar_len);
    args.push_back(value);

    return NC_OK;
}

void NcConf::yamlPopScalar()
{
    NcString &value = args[args.size() - 1];
    LOG_DEBUG("pop '%.*s'", value.length(), value.c_str());
    args.pop_back(); // 删除最后一个元素
}

rstatus_t NcConf::parseCore(NcConf *cf, NcConfPool *data)
{
    rstatus_t status = cf->yamlEventNext();
    if (status != NC_OK) 
    {
        return status;
    }

    LOG_DEBUG("next event %d depth %" PRIu32 " seq %d", 
        cf->event.type, cf->depth, cf->seq);

    bool done = false, leaf = false, new_pool = false;
    switch (cf->event.type) 
    {
    case YAML_MAPPING_END_EVENT:
        cf->depth--;
        if (cf->depth == 1) 
        {
            cf->yamlPopScalar();
        } 
        else if (cf->depth == 0) 
        {
            done = true;
        }
        break;

    case YAML_MAPPING_START_EVENT:
        cf->depth++;
        break;

    case YAML_SEQUENCE_START_EVENT:
        cf->seq = 1;
        break;

    case YAML_SEQUENCE_END_EVENT:
        cf->yamlPopScalar();
        cf->seq = 0;
        break;

    case YAML_SCALAR_EVENT:
        status = cf->yamlPushScalar();
        if (status != NC_OK) 
        {
            break;
        }
        if (cf->seq) 
        {
            leaf = true;
        } 
        else if (cf->depth == CONF_ROOT_DEPTH) 
        {
            data = new NcConfPool();
            cf->pool.push_back(data);
            new_pool = true;
        } 
        else if ((cf->args).size() == cf->depth + 1) 
        {
            leaf = true;
        }
        break;

    default:
        break;
    }

    cf->yamlEventDone();
    if (status != NC_OK) 
    {
        return status;
    }

    if (done) 
    {
        return NC_OK;
    }

    if (leaf || new_pool) 
    {
        status = handler(cf, data);
        if (leaf) 
        {
            cf->yamlPopScalar();
            if (!cf->seq) 
            {
                cf->yamlPopScalar();
            }
        }

        if (status != NC_OK) 
        {
            return status;
        }
    }

    return parseCore(cf, data);
}

rstatus_t NcConf::handler(NcConf *cf, NcConfPool *data)
{
    rstatus_t status;
    uint32_t narg = (cf->args).size();
    if (narg <= 1)
    {
        return NC_OK;
    }

    NcString value = (cf->args)[narg - 1];
    NcString key = (cf->args)[narg - 2];

    LOG_DEBUG("key : %s, value : %s", key.c_str(), value.c_str());

    if (key == (const uint8_t*)"listen")
    {
        status = data->listen.parse(value);
        if (status != NC_OK)
        {
            LOG_DEBUG("status : %d", status);
        }
    }
    else if (key == (const uint8_t*)"servers")
    {
        NcConfServer *cs = new NcConfServer();
        status = cs->parse(value);
        if (status != NC_OK)
        {
            LOG_DEBUG("status : %d", status);
            nc_delete(cs);
        }
        else
        {
            data->server.push_back(cs);
        }
    }
    else
    {
        // pass
    }    

    return NC_OK;
}

