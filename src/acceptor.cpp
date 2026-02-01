#include "acceptor.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <arpa/inet.h>

namespace ACCEPTOR_NAMESPACE
{
    Acceptor::Acceptor(const char *address, uint16_t port, IEventLoop &_eventLoop, onNewConnectionCallback _callback)
        : listen_fd(-1), eventloop(_eventLoop), new_connection_cb(_callback)
    {
        // Create socket
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0)
        {
            std::cerr << "Failed to create listening socket\n";
            return;
        }

        // Set non-blocking
        int flags = fcntl(listen_fd, F_GETFL, 0);
        if (flags == -1 || fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            std::cerr << "Failed to set non-blocking on listen socket\n";
            close(listen_fd);
            listen_fd = -1;
            return;
        }

        // Set SO_REUSEADDR
        int reuse = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        {
            std::cerr << "setsockopt failed\n";
            close(listen_fd);
            listen_fd = -1;
            return;
        }

        // Setup address
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0)
        {
            std::cerr << "Invalid address\n";
            close(listen_fd);
            listen_fd = -1;
            return;
        }

        // Bind
        if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
        {
            std::cerr << "Failed to bind to " << address << ":" << port << "\n";
            close(listen_fd);
            listen_fd = -1;
            return;
        }

        // Listen
        if (listen(listen_fd, 5) != 0)
        {
            std::cerr << "Listen failed\n";
            close(listen_fd);
            listen_fd = -1;
            return;
        }

        std::cout << "Acceptor listening on " << address << ":" << port << std::endl;
    }

    Acceptor::~Acceptor()
    {
        if (listen_fd != -1)
        {
            close(listen_fd);
        }
    }

    void Acceptor::handleRead()
    {
        while (true)
        {
            bool should_continue = acceptNewConnection();
            if (!should_continue)
                break;
        }
    }

    bool Acceptor::acceptNewConnection()
    {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(listen_fd, (sockaddr *)&client_addr, &client_addr_len);

        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return false; // No more connections to accept
            }
            perror("Accept failed");
            return false;
        }
        auto client = std::make_shared<Client>();
        client->fd = client_fd;

        if (new_connection_cb)
        {
            new_connection_cb(client);
        }

        return true; // Successfully accepted a connection, continue trying
    }

    void Acceptor::handleWrite()
    {
        // Acceptor doesn't handle write events
    }

    void Acceptor::handleClose()
    {
        eventloop.remove_event_handler(this);
        if (listen_fd != -1)
        {
            close(listen_fd);
            listen_fd = -1;
        }
    }

    void Acceptor::send(std::string &data)
    {
        // Acceptor doesn't send data
    }

    void Acceptor::start()
    {
        eventloop.register_event_handler(this, READ_EVENT);
    }
}
