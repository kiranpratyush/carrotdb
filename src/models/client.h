#pragma once
#include <string>
#include <memory>
#define CLIENT_BLOCKED 1 << 0
#define CLIENT_READY 1 << 1
#define CLIENT_READING 1 << 2
#define CLIENT_WRITING 1 << 3

#ifndef SERVER_NAMESPACE
#define SERVER_NAMESPACE server
#endif

namespace SERVER_NAMESPACE
{
    struct Client
    {
        std::string read_buffer{};
        std::string write_buffer{};

        Client() = default;

        explicit Client(unsigned int initial_capacity)
        {
            read_buffer.reserve(initial_capacity);
            write_buffer.reserve(initial_capacity);
        }
    };
    struct ClientContext
    {
        std::shared_ptr<Client> client{};
        int client_fd;
        unsigned long long current_read_position{};
        unsigned long long current_write_position{};
        bool isBlocked{false};
        int unblocked_client_fd{-1};
        ClientContext(std::shared_ptr<Client> client, int fd) : client{client}, client_fd{fd}
        {
        }
    };

}
