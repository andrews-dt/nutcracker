#include <nc_lua.h>

int main(int argc, char **argv)
{
    NcLua lua;

    int r = lua.loadFile("../scripts/test.lua");
    printf("test lua r : %d\n", r);

    NcString s = "hello, world";
    lua.pushNcString(&s);
    lua.pushInt(9999);

    return 0;
}