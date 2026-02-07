#ifndef SERVER_MODEL
#define SERVER_MODEL

#include <string>
#include <cstdint>

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
    std::string rdb_file_path{""};
    std::string rdb_file_name{""};
};
#endif