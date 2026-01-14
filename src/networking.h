#include <memory>
#include <cstring>
#include "models/client.h"
#include "utils/utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace SERVER_NAMESPACE;

namespace NETWORKING
{
    std::unique_ptr<Client> connect_client(std::string_view host, u_int16_t port, bool &isError);
    ssize_t read_client(int sockfd, Client *client);
    ssize_t write_client(int sockfd, Client *client);

}