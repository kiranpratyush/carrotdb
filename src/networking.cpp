#include "networking.h"

namespace NETWORKING
{
    bool connect_client(std::string_view host, u_int16_t port, int &resultsockfd)
    {
        // build the server address
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("Error opening a socket");
            return false;
        }
        resultsockfd = sockfd;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.data(), &serv_addr.sin_addr) < 0)
        {
            perror("Invalid address/address not supported");
            return false;
        }
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("Connection failed");
            return false;
        }
        return true;
        
    }

    ssize_t read_client(int sockfd, std::string &read_buffer)
    {
        char buffer[MAX_BUFFER_SIZE];
        ssize_t bytes_read = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);

        if (bytes_read > 0)
        {
            read_buffer.append(buffer, bytes_read);
        }

        return bytes_read;
    }

    ssize_t write_client(int sockfd, std::string &write_buffer)
    {
        if (write_buffer.empty())
        {
            return 0;
        }

        ssize_t bytes_sent = send(sockfd, write_buffer.c_str(),
                                  write_buffer.size(), 0);

        if (bytes_sent > 0)
        {
            write_buffer.erase(0, bytes_sent);
        }

        return bytes_sent;
    }
}
