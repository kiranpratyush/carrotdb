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
#include "models/client.h"
#include "replication.h"
#include "db/db.h"
#include "utils/utils.h"

namespace SERVER_NAMESPACE
{
    enum class ServerRole
    {
        MASTER,
        SLAVE
    };
    

    struct ServerConfig
    {
        ServerRole role{ServerRole::MASTER};
        int port{6379};
        std::string master_host{""};
        int master_port{0};
        std::string replication_id{"8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb"};
        uint64_t offset{0};
        uint8_t replicationStatus{0};
    };

    class Server
    {
    private:
        int epoll_instance_fd{-1};
        int server_fd{-1};
        int connection_backlog{5};
        struct sockaddr_in client_addr{};
        int client_addr_len{};
        struct sockaddr_in server_addr{};
        u_int retry_count{0};
        std::unordered_map<int, std::shared_ptr<Client>> active_clients{};
        std::unique_ptr<REPLICATION_NAMESPACE::MasterClient> master_client{};
        ServerConfig config{};

        int make_nonblocking(int &fd);
        int handle_write(int fd);
        int handle_read(int fd);
        int handle_new_client_connection();
        void handle_expired_blocked_clients();
        bool register_to_eventloop_for_read(int fd,bool is_first_time);
        bool register_to_eventloop_for_write(int fd,bool is_first_time);
    public:
        REDIS_NAMESPACE::DB db{};

    public:
        Server()
        {
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(6379);
            client_addr_len = sizeof(client_addr);
        }
        int setup();

        ~Server()
        {
            if (server_fd >= 0)
            {
                close(server_fd);
            }
        }
        void run(std::string &buffer, int buffer_size);
        void initialize(int argc, char **argv);

    private:
    };
}
