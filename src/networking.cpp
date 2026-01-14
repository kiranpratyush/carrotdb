#include "networking.h"

namespace NETWORKING
{
    std::unique_ptr<Client> connect_client(std::string_view host, u_int16_t port, bool &isError)
    {
        // build the server address
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("Error opening a socket");
            isError = true;
            return nullptr;
        }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.data(), &serv_addr.sin_addr) < 0)
        {
            perror("Invalid address/address not supported");
            isError = true;
            return nullptr;
        }
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("Connection failed");
            isError = true;
            return nullptr;
        }
        auto client = std::make_unique<Client>(MAX_BUFFER_SIZE);
        client->fd = sockfd;
        return client;
    }

    ssize_t read_client(int sockfd, Client *client)
    {
        char buffer[MAX_BUFFER_SIZE];
        ssize_t bytes_read = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);

        if (bytes_read > 0)
        {
            client->read_buffer.append(buffer, bytes_read);
        }

        return bytes_read;
    }

    ssize_t write_client(int sockfd, Client *client)
    {
        if (client->write_buffer.empty())
        {
            return 0;
        }

        ssize_t bytes_sent = send(sockfd, client->write_buffer.c_str(),
                                  client->write_buffer.size(), 0);

        if (bytes_sent > 0)
        {
            client->write_buffer.erase(0, bytes_sent);
        }

        return bytes_sent;
    }
}
