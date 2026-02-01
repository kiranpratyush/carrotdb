#include "replication.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "parser/command-parser.h"
#include "db/db.h"
#include "connection.h"
#include "master-connection.h"

using namespace REDIS_NAMESPACE;
using namespace CONNECTION_NAMEAPACE;
using namespace MASTER_CONNECTION_NAMESPACE;

namespace
{
    std::vector<char> read_rdb_file(std::string_view filepath)
    {
        std::ifstream file(filepath.data(), std::ios::binary | std::ios::ate);
        if (!file)
        {
            std::cerr << "Error: Could not open RDB file: " << filepath << std::endl;
            return {};
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size))
        {
            std::cerr << "Error: Could not read RDB file content." << std::endl;
            return {};
        }
        file.close();
        return buffer;
    }
}

namespace REPLICATION_NAMESPACE
{
    void ReplicationManager::handle_info(ClientContext &c, ServerConfig &config)
    {
        std::string role_info;
        bool is_master{false};
        std::string master_replid{};
        std::string offset{};
        if (config.role == ServerRole::MASTER)
        {
            role_info = "role:master\r\n";
            is_master = true;
            master_replid = "master_replid:" + config.replication_id + "\r\n";
            offset = "master_repl_offset:" + std::to_string(config.offset) + "\r\n";
        }
        else
        {
            role_info = "role:slave\r\n";
        }
        if (is_master)
        {
            role_info += master_replid;
            role_info += offset;
        }
        encode_bulk_string(&c.client->write_buffer, role_info);
        return;
    }
    void ReplicationManager::handle_replconf(ClientContext &c, ServerConfig &config)
    {
        auto *replconf_cmd = static_cast<ReplConfCommand *>(c.command.get());
        if (replconf_cmd->subcommand == ReplConfType::ACK)
        {
            c.client->replica_offset = replconf_cmd->ack_offset;
            check_blocked_wait_clients(c);
            return;
        }
        encode_simple_string(&c.client->write_buffer, "OK");
        return;
    }

    void ReplicationManager::handle_psync(ClientContext &c, ServerConfig &config)
    {
        encode_simple_string(&c.client->write_buffer, "FULLRESYNC 12345 0");
        std::vector<char> rdb_content = read_rdb_file(config.rdb_file_path);

        std::string length_prefix = "$" + std::to_string(rdb_content.size()) + "\r\n";

        c.client->write_buffer.insert(
            c.client->write_buffer.end(),
            length_prefix.begin(),
            length_prefix.end());
        c.client->write_buffer.insert(
            c.client->write_buffer.end(),
            rdb_content.begin(),
            rdb_content.end());

        if (rdb_content.empty())
        {
            std::cerr << "Warning: RDB content is empty" << std::endl;
        }

        // Only register slave if not already registered
        if (!c.client->isslave)
        {
            c.client->isslave = true;

            // Register the TcpConnection if provided
            if (c.connection)
            {
                addSlaveConnection(c.client_fd, c.connection);
            }
        }

        return;
    }

    void ReplicationManager::handle_wait(ClientContext &c, ServerConfig &config)
    {
        auto *wait_cmd = static_cast<WaitCommand *>(c.command.get());
        uint64_t num_replicas_needed = wait_cmd->num_replica;
        uint64_t timeout_ms = wait_cmd->timeout;

        if (config.offset == 0)
        {
            uint64_t total_active_replicas = slave_connections.size();
            std::cout << "[handle_wait] No writes propagated, returning " << total_active_replicas << std::endl;
            c.client->write_buffer.append(":" + std::to_string(total_active_replicas) + "\r\n");
            return;
        }
        uint64_t already_acked = 0;
        for (auto &[fd, conn] : slave_connections)
        {
            if (auto slave = conn->getClient())
            {
                if (slave->replica_offset >= static_cast<int64_t>(config.offset))
                {
                    already_acked++;
                }
            }
        }

        if (already_acked >= num_replicas_needed)
        {
            std::cout << "[handle_wait] Already have " << already_acked << " replicas caught up" << std::endl;
            c.client->write_buffer.append(":" + std::to_string(already_acked) + "\r\n");
            return;
        }

        std::string getack_cmd = "*3\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n$1\r\n*\r\n";

        for (auto &[fd, conn] : slave_connections)
        {
            if (auto slave = conn->getClient())
            {
                slave->write_buffer.insert(
                    slave->write_buffer.end(),
                    getack_cmd.begin(),
                    getack_cmd.end());
                conn->flush();
            }
        }

        c.isBlocked = true;
        blocked_wait_clients.emplace_back(
            c.client, c.client_fd, timeout_ms, num_replicas_needed, static_cast<int64_t>(config.offset));
    }

    void ReplicationManager::check_blocked_wait_clients(ClientContext &c)
    {
        if (blocked_wait_clients.empty())
            return;

        auto it = blocked_wait_clients.begin();
        while (it != blocked_wait_clients.end())
        {
            auto blocked_client = it->client.lock();
            if (!blocked_client)
            {
                it = blocked_wait_clients.erase(it);
                continue;
            }

            uint64_t acked_replicas = 0;
            for (auto &[fd, conn] : slave_connections)
            {
                if (auto slave = conn->getClient())
                {
                    if (slave->replica_offset >= it->master_offset)
                    {
                        acked_replicas++;
                    }
                }
            }

            std::cout << "[check_blocked_wait] acked=" << acked_replicas
                      << " needed=" << it->num_replicas_needed << std::endl;

            if (acked_replicas >= it->num_replicas_needed)
            {
                // Unblock the client with success
                blocked_client->write_buffer.append(":" + std::to_string(acked_replicas) + "\r\n");
                c.unblocked_client_fd = it->client_fd;
                std::cout << "[check_blocked_wait] Unblocking client fd=" << it->client_fd
                          << " with " << acked_replicas << " replicas" << std::endl;
                it = blocked_wait_clients.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::vector<int> ReplicationManager::check_and_expire_blocked_wait_clients()
    {
        std::vector<int> expired_fds;

        auto it = blocked_wait_clients.begin();
        while (it != blocked_wait_clients.end())
        {
            if (it->is_expired())
            {
                auto blocked_client = it->client.lock();
                if (blocked_client)
                {
                    uint64_t acked_replicas = 0;
                    for (auto &[fd, conn] : slave_connections)
                    {
                        if (auto slave = conn->getClient())
                        {
                            if (slave->replica_offset >= it->master_offset)
                            {
                                acked_replicas++;
                            }
                        }
                    }
                    blocked_client->write_buffer.append(":" + std::to_string(acked_replicas) + "\r\n");
                    expired_fds.push_back(it->client_fd);
                    std::cout << "[expire_blocked_wait] Timeout for client fd=" << it->client_fd
                              << " returning " << acked_replicas << " replicas" << std::endl;
                }
                it = blocked_wait_clients.erase(it);
            }
            else
            {
                ++it;
            }
        }

        return expired_fds;
    }

    int ReplicationManager::get_next_wait_timeout_ms()
    {
        if (blocked_wait_clients.empty())
            return -1;

        auto now = std::chrono::steady_clock::now();
        int min_timeout = -1;

        for (const auto &blocked : blocked_wait_clients)
        {
            if (blocked.timeout_at == std::chrono::steady_clock::time_point::max())
                continue;

            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 blocked.timeout_at - now)
                                 .count();

            if (remaining <= 0)
                return 0;

            if (min_timeout == -1 || remaining < min_timeout)
                min_timeout = static_cast<int>(remaining);
        }

        return min_timeout;
    }

    bool ReplicationManager::handle(ClientContext &c, ServerConfig &config)
    {
        if (!c.command)
        {
            std::cerr << "No command in ClientContext" << std::endl;
            return false;
        }

        if (c.command->type == CommandType::INFO)
        {
            handle_info(c, config);
            return true;
        }

        if (c.command->type == CommandType::REPLCONF)
        {
            handle_replconf(c, config);
            return true;
        }
        if (c.command->type == CommandType::PSYNC)
        {
            handle_psync(c, config);
            return true;
        }
        if (c.command->type == CommandType::WAIT)
        {
            handle_wait(c, config);
            return true;
        }
        return false;
    }
    bool ReplicationManager::handle_propagate(ClientContext &c, ServerConfig &config)
    {
        std::cout << "[handle_propagate] slave_connections.size()=" << slave_connections.size() << std::endl;
        if (slave_connections.empty())
            return false;

        if (!c.command)
            return false;

        if (c.command->is_write_command())
        {
            std::string raw_resp_command = c.command->to_resp();

            config.offset += c.command->bytes_processed;

            std::vector<int> inactive_fds;

            for (auto &[fd, conn] : slave_connections)
            {
                if (!conn || !conn->getClient())
                {
                    std::cout << "[handle_propagate] Slave fd=" << fd << " is inactive, marking for removal" << std::endl;
                    inactive_fds.push_back(fd);
                    continue;
                }

                if (auto client = conn->getClient())
                {
                    std::cout << "[handle_propagate] Adding " << raw_resp_command.size()
                              << " bytes to slave fd=" << fd << std::endl;
                    client->write_buffer.insert(
                        client->write_buffer.end(),
                        raw_resp_command.begin(),
                        raw_resp_command.end());
                }
            }

            // Remove inactive connections
            for (int fd : inactive_fds)
            {
                removeSlaveConnection(fd);
            }

            return true;
        }

        return false;
    }

    void ReplicationManager::addSlaveConnection(int fd, CONNECTION_NAMEAPACE::TcpConnection *conn)
    {
        std::cout << "[ReplicationManager] Registering slave connection fd=" << fd << std::endl;
        slave_connections[fd] = conn;
    }

    void ReplicationManager::removeSlaveConnection(int fd)
    {
        std::cout << "[ReplicationManager] Unregistering slave connection fd=" << fd << std::endl;
        slave_connections.erase(fd);
    }

    void ReplicationManager::propagateAndNotifySlaves(ClientContext &c, ServerConfig &config)
    {
        std::cout << "[MASTER] propagateAndNotifySlaves called" << std::endl;

        bool propagated = handle_propagate(c, config);
        std::cout << "[MASTER] handle_propagate returned: " << propagated << std::endl;

        for (auto &[fd, conn] : slave_connections)
        {
            if (auto client = conn->getClient())
            {
                std::cout << "[MASTER] Checking slave fd=" << fd
                          << " write_buffer size=" << client->write_buffer.size() << std::endl;

                if (!client->write_buffer.empty())
                {
                    std::cout << "[MASTER] Slave " << fd << " is scheduled for write: "
                              << client->write_buffer.size() << " bytes" << std::endl;

                    conn->flush();
                }
            }
        }
    }
}
