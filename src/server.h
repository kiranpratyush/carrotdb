#pragma once
#include <sys/types.h>
#include <sys/epoll.h>
#include <memory>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <cassert>
#include <fcntl.h>
#include <errno.h>
#include <unordered_map>
#include "models/server-model.h"
#include "models/client.h"
#include "replication.h"
#include "db/db.h"
#include "utils/utils.h"
#include "eventloop.h"
#include "connection.h"
#include "acceptor.h"
#include "master-connection.h"
#include "rdb.h"
#include "acl.h"

namespace SERVER_NAMESPACE
{
    class Server
    {
    private:
        std::unique_ptr<EVENT_LOOP_NAMESPACE::EpollEventLoop> eventloop;
        std::unique_ptr<ACCEPTOR_NAMESPACE::Acceptor> acceptor;
        std::unique_ptr<MASTER_CONNECTION_NAMESPACE::MasterConnection> master_connection;
        std::unordered_map<int, std::shared_ptr<CONNECTION_NAMEAPACE::TcpConnection>> active_connections{};

        ServerConfig config;
        REPLICATION_NAMESPACE::ReplicationManager replicationManager{};
        ACL_NAMESPACE::ACLManager aclManager{};
        RDB_NAMESPACE::Rdb rdb;

        void handle_expired_blocked_clients();
        void handle_master_disconnection();

        void onNewConnection(std::shared_ptr<Client> client);
        void onMessage(CONNECTION_NAMEAPACE::TcpConnection *conn);
        void processClientRequest(CONNECTION_NAMEAPACE::TcpConnection *conn);

    public:
        std::shared_ptr<REDIS_NAMESPACE::DB> db;

    public:
        Server() : config{}, db{std::make_shared<REDIS_NAMESPACE::DB>()},
                   rdb{}
        {
            rdb.setDBInstance(db);
        };
        int setup();
        ~Server() = default;
        void run(std::string &buffer, int buffer_size);
        void initialize(int argc, char **argv);
    };
}
