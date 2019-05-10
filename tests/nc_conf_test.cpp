#include <nc_conf.h>

int main(int argc, char **argv)
{
    NcString key("listen", strlen("listen"));
    NcString value("127.0.0.1:22121", strlen("127.0.0.1:22121"));

    uint8_t *p = (uint8_t*)value.c_str() + value.length() -1;
    uint8_t *start = (uint8_t*)value.c_str();
    LOG_DEBUG("start : %s", start);
    uint8_t *q = nc_strrchr(p, start, ':');
    LOG_DEBUG("q : %p", q);

    LOG_DEBUG("status : %d", key == "listen");

    // NcConf conf1;
    // conf1.openFile("../conf/nutcracker.leaf.yml");
    // conf1.parseProcess();
    // conf1.dump();

    // NcConf conf2;
    // conf2.openFile("../conf/nutcracker.root.yml");
    // conf2.parseProcess();
    // conf2.dump();

    NcConf conf3;
    conf3.openFile("../conf/nutcracker.yml");
    conf3.parseProcess();
    conf3.dump();

    return 0;
}