#pragma once
#include <memory>
#include <cstring>
#include "utils/utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>


namespace NETWORKING
{
    bool connect_client(std::string_view host, u_int16_t port, int &sockfd);
    ssize_t read_client(int sockfd, std::string &read_buffer);
    ssize_t write_client(int sockfd,std::string &write_buffer);

}