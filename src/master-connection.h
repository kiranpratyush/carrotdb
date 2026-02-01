#ifndef MASTER_CONNECTION_H
#define MASTER_CONNECTION_H

#include <string>
#include <memory>
#include <functional>
#include "models/IEventLoop.h"
#include "models/server-model.h"
#include "db/db.h"
#include "slave-replication.h"

using namespace SERVER_NAMESPACE;
using namespace REDIS_NAMESPACE;
using namespace SLAVE_REPLICATION_NAMESPACE;

namespace MASTER_CONNECTION_NAMESPACE
{
    using DisconnectCallback = std::function<void()>;

    class MasterConnection : public IEventHandler
    {
    private:
        std::unique_ptr<SlaveReplicationClient> slave_client;
        IEventLoop &eventloop;
        DB *db;
        ServerConfig *config;
        bool connected;
        DisconnectCallback disconnect_callback;

    public:
        MasterConnection(std::string_view host, uint16_t port, uint16_t slave_port,
                        IEventLoop &_eventloop, DB *_db, ServerConfig *_config);
        ~MasterConnection();

        inline int getfd() override
        {
            return slave_client ? slave_client->fd : -1;
        }

        void handleRead() override;
        void handleWrite() override;
        void handleClose() override;
        void send(std::string &data) override;

        bool connect();
        bool isConnected() const { return connected; }
        void setDisconnectCallback(DisconnectCallback callback) { disconnect_callback = callback; }
        
        SlaveReplicationClient* getSlaveClient() { return slave_client.get(); }
    };
}

#endif
