#include "networking.h"

namespace NETWORKING
{
    bool connect_client(std::string_view host, u_int16_t port, int &resultsockfd)
    {
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
}
