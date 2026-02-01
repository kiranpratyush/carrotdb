#pragma once
#include <functional>
#include <memory>
#include "models/client.h"
#include "models/server-model.h"
#include "utils/utils.h"
#include "parser/parser.h"
#include "networking.h"
#include "db/db.h"

using namespace SERVER_NAMESPACE;
using namespace REDIS_NAMESPACE;

// Forward declarations
namespace CONNECTION_NAMEAPACE
{
    class TcpConnection;
}

namespace REPLICATION_NAMESPACE
{
    class ReplicationManager
    {
    private:
        std::unordered_map<int, CONNECTION_NAMEAPACE::TcpConnection *> slave_connections;
        std::vector<BlockedWaitClient> blocked_wait_clients;

        void handle_info(ClientContext &c, ServerConfig &config);
        void handle_replconf(ClientContext &c, ServerConfig &config);
        void handle_psync(ClientContext &c, ServerConfig &config);
        void handle_wait(ClientContext &c, ServerConfig &config);

    public:
        bool handle(ClientContext &c, ServerConfig &config);
        bool handle_propagate(ClientContext &c, ServerConfig &config);
        void check_blocked_wait_clients(ClientContext &c);
        std::vector<int> check_and_expire_blocked_wait_clients();
        int get_next_wait_timeout_ms();

        void addSlaveConnection(int fd, CONNECTION_NAMEAPACE::TcpConnection *conn);
        void removeSlaveConnection(int fd);
        void propagateAndNotifySlaves(ClientContext &c, ServerConfig &config);
    };
}