#include <nc_log.h>

int main(int argc, char **argv)
{
    // NcLogger::getInstance().init(LLOG_VVVERB, "./test.logs");

    // LOG_DEBUG("test ------- 1");
    // LOG_ERROR("test ------- 2");

    const char *s1 = "../a.cpp1xxxaaa";

    int pos = NcLogger::stringLastOf(s1, '/');
    printf("pos : %d\n", pos);

    char filetemp[256];
    int filetemp_len = pos + 1;
    strncpy(filetemp, s1 + filetemp_len, strlen(s1) - filetemp_len + 1);

    printf("filetemp : %s\n", filetemp);

    const char *s2 = "../a.cpp";

    pos = NcLogger::stringLastOf(s2, '/');
    printf("pos : %d\n", pos);

    filetemp_len = pos + 1;
    strncpy(filetemp, s2 + filetemp_len, strlen(s2) - filetemp_len + 1);

    printf("filetemp : %s\n", filetemp);

    return 0;
}