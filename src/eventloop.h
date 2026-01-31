
#include <cstdlib>
#include <cstdio>
#include <memory>

#include "connection.h"
#include "models/IEventLoop.h"
#include <sys/epoll.h>

using namespace CONNECTION_NAMEAPACE;

namespace EVENT_LOOP_NAMESPACE
{

    class EpollEventLoop : public IEventLoop
    {
        int epoll_instance_fd;

    public:
        EpollEventLoop()
        {
            epoll_instance_fd = epoll_create1(0);
            if (epoll_instance_fd == -1)
            {
                perror("epoll create failed");
                exit(1);
            }
        }
        inline void register_event_handler(IEventHandler *eventHandler, uint32_t event) override
        {
            epoll_event ev{};
            ev.events = event;
            ev.data.ptr = eventHandler;
            if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, eventHandler->getfd(), &ev) == -1)
            {
                perror("Failed to register event");
                return;
            }
        }
        inline void remove_event_handler(IEventHandler *eventHandler) override
        {
            if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_DEL, eventHandler->getfd(), nullptr) == -1)
            {
                perror("Failed to remove event handler");
                return;
            }
        }
        inline void update_event_handler(IEventHandler *eventHandler, uint32_t event) override
        {
            epoll_event ev{};
            ev.events = event;
            ev.data.ptr = eventHandler;
            if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_MOD, eventHandler->getfd(), &ev) == -1)
            {
                perror("Failed to update event");
                return;
            }
        }
        void run(uint32_t time_ms) override;
    };

}