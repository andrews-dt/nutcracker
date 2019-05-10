#include <nc_queue.h>
#include <nc_log.h>

void print(NcQueue<int> &q)
{
    while (!q.empty())
    {
        int a = q.front();
        LOG_DEBUG("a = %d", a);
        q.pop();
    }
}

int main(int argc, char **argv)
{
    NcLogger::getInstance().init(LLOG_VVVERB, "./test.logs");

    NcQueue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.push(4);
    queue.push(5);
    // print(queue);

    int a = 4;
    NcQueue<int>::ConstIterator iter = queue.find(a);
    if (iter != queue.end())
    {
        LOG_DEBUG("*iter = %d", *iter);
        LOG_DEBUG("*iter = %d", *--iter);
        LOG_DEBUG("*iter = %d", *--iter);
        LOG_DEBUG("*iter = %d", *--iter);
        LOG_DEBUG("*iter = %d", *++iter);
        LOG_DEBUG("*iter = %d", *++iter);
        LOG_DEBUG("*iter = %d", *++iter);
    }

    LOG_DEBUG("begin = %p", queue.begin());
    LOG_DEBUG("end = %p", queue.end());

    return 0;
}