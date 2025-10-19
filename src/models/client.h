#pragma once

#include <string>

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
        Client &client;
        unsigned long long current_read_position{};
        unsigned long long current_write_position{};
        ClientContext(Client &c) : client{c}
        {
        }
    };

}
