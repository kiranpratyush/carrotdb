#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <functional>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include "models/IEventLoop.h"
#include "models/client.h"

using namespace SERVER_NAMESPACE;

namespace ACCEPTOR_NAMESPACE
{
    class Acceptor;
    using onNewConnectionCallback = std::function<void(std::shared_ptr<Client>)>;

    class Acceptor : public IEventHandler
    {
    private:
        int listen_fd;
        IEventLoop &eventloop;
        onNewConnectionCallback new_connection_cb;

        bool acceptNewConnection();

    public:
        Acceptor(const char *address, uint16_t port, IEventLoop &_eventLoop, onNewConnectionCallback _callback);
        ~Acceptor();

        bool isValid() const { return listen_fd != -1; }

        inline int getfd() override
        {
            return listen_fd;
        }

        void handleRead() override;
        void handleWrite() override;
        void handleClose() override;
        void send(std::string &data) override;

        void start();
    };
}

#endif
