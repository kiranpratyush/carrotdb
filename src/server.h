#pragma once
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <cassert>
#include <fcntl.h>
#include <errno.h>

namespace SERVER_NAMESPACE
{
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
        int make_nonblocking(int &fd);
        int handle_write(int fd);
        int handle_read(int fd);
        int handle_new_client_connection();

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
    };
}
