#ifndef CONNECTION
#define CONNECTION
#include <unistd.h>
#include <functional>
#include <string>
#include <memory>
#include "models/client.h"
#include "models/IEventLoop.h"

using namespace SERVER_NAMESPACE;

namespace CONNECTION_NAMEAPACE
{

    class TcpConnection;
    using onMessageCallBack = std::function<void(TcpConnection *)>;

    class TcpConnection : public IEventHandler
    {
    private:
        std::shared_ptr<Client> client;
        IEventLoop &eventloop;
        onMessageCallBack cb;

    public:
        TcpConnection(std::shared_ptr<Client> _client, IEventLoop &_eventLoop, onMessageCallBack _callback);

        inline int getfd() override
        {
            return client->fd;
        }

        inline std::shared_ptr<Client> getClient()
        {
            return client;
        }

        void handleWrite() override;
        void handleRead() override;
        void send(std::string &) override;
        void flush(); // Trigger write for existing buffer data
        void handleClose();
        void handleError();
    };

}
#endif