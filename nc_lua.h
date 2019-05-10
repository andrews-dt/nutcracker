#ifndef _NC_LUA_H_
#define _NC_LUA_H_

#include <stdio.h>
#include <lua.hpp> 

class NcLua
{
public:
    NcLua()
    {
        m_luastate_ = lua_open();
    }

    int loadFile(const char *name)
    {
        if (name != NULL)
        {
            return luaL_loadfile(m_luastate_, name);
        }
        
        return -1;
    }

    void pushNcString(NcString *s)
    {
        lua_pushlstring(m_luastate_, s->c_str(), s->length());
    }

    void pushInt(int n)
    {
        lua_pushinteger(m_luastate_, n);
    }

    void pushString(const char *s, size_t len)
    {
        lua_pushlstring(m_luastate_, s, len);
    }

    int execLua(int args_n, int ret_n)
    {
        return 0;
    }

private:
    lua_State *m_luastate_;
};

#endif