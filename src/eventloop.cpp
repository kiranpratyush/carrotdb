#include "eventloop.h"

namespace EVENT_LOOP_NAMESPACE
{

    void EpollEventLoop::run(int32_t timems)
    {
        epoll_event events[64]; /*Hard coded for now*/
        int n = epoll_wait(epoll_instance_fd, events, 64, timems);
        for (int i = 0; i < n; i++)
        {
            auto handler = static_cast<IEventHandler *>(events[i].data.ptr);
            auto mask = events[i].events;
            if (mask & (EPOLLIN | EPOLLHUP | EPOLLERR))
                handler->handleRead();
            if (mask & EPOLLOUT)
                handler->handleWrite();
        }
    }
}