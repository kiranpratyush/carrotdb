
#include <memory>
#include <cstdint>
#include <sys/epoll.h>

constexpr uint32_t READ_EVENT = EPOLLIN | EPOLLET;
constexpr uint32_t WRITE_EVENT = EPOLLOUT | EPOLLET;
constexpr uint32_t NO_EVENT = 0;

class IEventHandler
{
public:
    ~IEventHandler() = default;
    virtual void handleRead() = 0;
    virtual void handleWrite() = 0;
    virtual void handleClose() = 0;
    virtual int getfd() = 0;
    virtual void send(std::string &data) = 0;
};

class IEventLoop
{
public:
    ~IEventLoop() = default;
    virtual void register_event_handler(IEventHandler *, uint32_t) = 0;
    virtual void update_event_handler(IEventHandler *, uint32_t) = 0;
    virtual void remove_event_handler(IEventHandler *) = 0;
    virtual void run(uint32_t time_ms) = 0;
};
