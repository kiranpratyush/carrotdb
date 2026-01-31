#include "replication.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "parser/command-parser.h"
#include "db/db.h"

using namespace REDIS_NAMESPACE;

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
{ /*This is on slave*/
    void MasterClient::set_db_and_config(REDIS_NAMESPACE::DB *database, ServerConfig *server_config)
    {
        db = database;
        config = server_config;
    }

    void MasterClient::handle_master(bool is_read_event, bool &should_invert)
    {
        std::cout << "[handle_master] is_read=" << is_read_event
                  << " handshake_completed=" << is_handshake_completed << std::endl;
        if (!is_handshake_completed)
        {
            handle_handshake(is_read_event, should_invert);
            // After handshake completes, check if there are commands in buffer
            if (is_handshake_completed && !read_buffer.empty())
            {
                std::cout << "[handle_master] Handshake done, processing remaining buffer" << std::endl;
                bool needs_response = handle_commands();
                should_invert = needs_response;
            }
        }
        else if (is_read_event)
        {
            std::cout << "[handle_master] Calling handle_commands()" << std::endl;
            bool needs_response = handle_commands();
            should_invert = needs_response;
        }
        else
        {
            std::cout << "[handle_master] Write event, writing buffer" << std::endl;
            NETWORKING::write_client(fd, write_buffer);
            should_invert = true;
        }
    }

    bool MasterClient::handle_commands()
    {
        bool needs_response = false;
        std::cout << "[handle_commands] Reading from master fd=" << fd << std::endl;
        ssize_t bytes = NETWORKING::read_client(fd, read_buffer);
        std::cout << "[handle_commands] Read " << bytes << " bytes, buffer size=" << read_buffer.size() << std::endl;

        if (read_buffer.empty())
        {
            std::cout << "[handle_commands] Buffer is empty, returning" << std::endl;
            return false;
        }

        std::cout << "Slave received from master: " << read_buffer << std::endl;

        /*TODO: we have to move the parsing of command inside the client_context and figure out how to handle partial command parsing*/
        while (!read_buffer.empty() && db && config)
        {
            auto client_context = ClientContext{std::shared_ptr<Client>(this, [](Client *) {}), fd};
            auto command = CommandParser::parseCommand(client_context);
            size_t command_bytes = client_context.current_read_position;

            if (command->type == CommandType::REPLCONF)
            {
                auto *replconf_cmd = static_cast<ReplConfCommand *>(command.get());
                if (replconf_cmd->subcommand == ReplConfType::GETACK)
                {
                    std::cout << "[handle_commands] Received GETACK, responding with ACK " << bytes_processed << std::endl;
                    // TODO , temporarily cleaning up write buffer,need to figure out better approach
                    write_buffer.clear();
                    encode_array_header(&write_buffer, 3);
                    encode_bulk_string(&write_buffer, "REPLCONF");
                    encode_bulk_string(&write_buffer, "ACK");
                    encode_bulk_string(&write_buffer, std::to_string(bytes_processed));

                    bytes_processed += client_context.current_read_position;
                    read_buffer.erase(0, command_bytes);
                    needs_response = true;
                    continue;
                }
            }
            client_context.current_read_position = 0;
            db->execute(client_context, config);
            bytes_processed += client_context.current_read_position;
            std::cout << "[handle_commands] bytes_processed now=" << bytes_processed << std::endl;

            if (client_context.current_read_position == 0)
                break;

            read_buffer.erase(0, client_context.current_read_position);
        }
        return needs_response;
    }

    void MasterClient::handle_handshake(bool is_read_event, bool &should_invert)
    {
        if (is_read_event)
        {
            NETWORKING::read_client(fd, read_buffer);
            std::string response;
            bool status = Parser::parse_simple_string(read_buffer, response);
            if (!status)
                return;

            should_invert = true;
            if (replication_status == ReplicationStatus::PING_SENT && is_equal(response, "PONG"))
                replication_status = ReplicationStatus::PING_SENT_SUCCESS;
            else if (replication_status == ReplicationStatus::REPLCONF_PORT_SENT && is_equal(response, "OK"))
                replication_status = ReplicationStatus::REPLCONF_PORT_SUCCESS;
            else if (replication_status == ReplicationStatus::REPLCONF_CAPA_SENT && is_equal(response, "OK"))
            {
                replication_status = ReplicationStatus::REPLCONF_CAPA_SUCCESS;
            }
            else if (replication_status == ReplicationStatus::PSYNC_SENT)
            {
                replication_status = ReplicationStatus::HANDSHAKE_SUCCESS;
                is_handshake_completed = true;
                // Find and skip the RDB file data
                // RDB is sent as $<length>\r\n<data> (no trailing \r\n)
                size_t rdb_start = read_buffer.find("$");
                if (rdb_start != std::string::npos)
                {
                    std::cout << "RDB file data received in handshake, processing..." << std::endl;
                    // Parse the length after $
                    size_t length_start = rdb_start + 1;
                    size_t length_end = read_buffer.find("\r\n", length_start);
                    if (length_end != std::string::npos)
                    {
                        std::string length_str = read_buffer.substr(length_start, length_end - length_start);
                        unsigned long long rdb_length = 0;
                        if (convert_positive_string_to_number(length_str, rdb_length))
                        {
                            // Skip past $<length>\r\n<rdb_data>
                            size_t rdb_data_start = length_end + 2; // Skip \r\n after length
                            size_t rdb_end = rdb_data_start + rdb_length;

                            if (rdb_end <= read_buffer.size())
                            {
                                // There might be commands after the RDB data
                                read_buffer.erase(0, rdb_end);
                                std::cout << "RDB skipped, remaining buffer size: " << read_buffer.size() << std::endl;
                                if (!read_buffer.empty())
                                {
                                    std::cout << "Remaining buffer: " << read_buffer << std::endl;
                                }
                            }
                            else
                            {
                                // RDB data is incomplete or exact, clear what we have
                                read_buffer.clear();
                            }
                        }
                        else
                        {
                            read_buffer.clear();
                        }
                    }
                    else
                    {
                        read_buffer.clear();
                    }
                }
                else
                {
                    read_buffer.clear();
                }
                should_invert = false;
                return;
            }
            read_buffer.clear();
        }
        if (!is_read_event)
        {
            if (replication_status == ReplicationStatus::IDLE)
            {
                // send ping handshake here
                encode_array_header(&write_buffer, 1);
                encode_bulk_string(&write_buffer, "PING");
                replication_status = ReplicationStatus::PING_SENT;
            }
            if (replication_status == ReplicationStatus::PING_SENT_SUCCESS)
            {
                encode_array_header(&write_buffer, 3);
                encode_bulk_string(&write_buffer, "REPLCONF");
                encode_bulk_string(&write_buffer, "listening-port");
                encode_bulk_string(&write_buffer, std::to_string(port));
                replication_status = ReplicationStatus::REPLCONF_PORT_SENT;
            }
            if (replication_status == ReplicationStatus::REPLCONF_PORT_SUCCESS)
            {
                encode_array_header(&write_buffer, 3);
                encode_bulk_string(&write_buffer, "REPLCONF");
                encode_bulk_string(&write_buffer, "capa");
                encode_bulk_string(&write_buffer, "psync2");
                replication_status = ReplicationStatus::REPLCONF_CAPA_SENT;
            }
            if (replication_status == ReplicationStatus::REPLCONF_CAPA_SUCCESS)
            {
                encode_array_header(&write_buffer, 3);
                encode_bulk_string(&write_buffer, "PSYNC");
                encode_bulk_string(&write_buffer, repl_id);
                encode_bulk_string(&write_buffer, std::to_string(offset));
                replication_status = ReplicationStatus::PSYNC_SENT;
            }
            NETWORKING::write_client(fd, write_buffer);
            should_invert = true;
        }
    }

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
        // Parse the command to check if it's an ACK
        auto command = CommandParser::parseCommand(c);
        c.current_read_position = 0; // Reset for potential re-parsing

        if (command->type == CommandType::REPLCONF)
        {
            auto *replconf_cmd = static_cast<ReplConfCommand *>(command.get());
            if (replconf_cmd->subcommand == ReplConfType::ACK)
            {
                // Store the replica's acknowledged offset
                c.client->replica_offset = replconf_cmd->ack_offset;
                std::cout << "[handle_replconf] Received ACK from replica fd=" << c.client_fd
                          << " offset=" << replconf_cmd->ack_offset << std::endl;

                // Check if any blocked WAIT clients can now be unblocked
                check_blocked_wait_clients(c);

                // No response needed for ACK
                return;
            }
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

        c.client->isslave = true;
        slave_clients.push_back(c.client);
        return;
    }

    void ReplicationManager::handle_wait(ClientContext &c, ServerConfig &config)
    {
        // Parse the WAIT command to get num_replica and timeout
        auto command = CommandParser::parseCommand(c);
        c.current_read_position = 0;

        if (command->type != CommandType::WAIT)
        {
            c.client->write_buffer.append(":0\r\n");
            return;
        }

        auto *wait_cmd = static_cast<WaitCommand *>(command.get());
        uint64_t num_replicas_needed = wait_cmd->num_replica;
        uint64_t timeout_ms = wait_cmd->timeout;

        std::cout << "[handle_wait] num_replicas=" << num_replicas_needed
                  << " timeout=" << timeout_ms << "ms master_offset=" << config.offset << std::endl;

        // If no writes have been propagated, return current replica count immediately
        if (config.offset == 0)
        {
            uint64_t total_active_replicas = 0;
            for (auto &client : slave_clients)
            {
                if (client.lock())
                {
                    total_active_replicas += 1;
                }
            }
            std::cout << "[handle_wait] No writes propagated, returning " << total_active_replicas << std::endl;
            c.client->write_buffer.append(":" + std::to_string(total_active_replicas) + "\r\n");
            return;
        }

        // Check if we already have enough replicas that have caught up
        uint64_t already_acked = 0;
        for (auto &weak_slave : slave_clients)
        {
            if (auto slave = weak_slave.lock())
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

        // Send REPLCONF GETACK * to all replicas (put in their write_buffer)
        std::string getack_cmd = "*3\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n$1\r\n*\r\n";

        for (auto &weak_slave : slave_clients)
        {
            if (auto slave = weak_slave.lock())
            {
                slave->write_buffer.insert(
                    slave->write_buffer.end(),
                    getack_cmd.begin(),
                    getack_cmd.end());
                std::cout << "[handle_wait] Queued GETACK to slave fd=" << slave->fd << std::endl;
            }
        }

        // Block the client and wait for ACKs
        c.isBlocked = true;
        blocked_wait_clients.emplace_back(
            c.client, c.client_fd, timeout_ms, num_replicas_needed, static_cast<int64_t>(config.offset));
        std::cout << "[handle_wait] Client blocked, waiting for " << num_replicas_needed << " replicas" << std::endl;
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

            // Count how many replicas have caught up to the required offset
            uint64_t acked_replicas = 0;
            for (auto &weak_slave : slave_clients)
            {
                if (auto slave = weak_slave.lock())
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
                    // Count current acked replicas for timeout response
                    uint64_t acked_replicas = 0;
                    for (auto &weak_slave : slave_clients)
                    {
                        if (auto slave = weak_slave.lock())
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
                return 0; // Already expired

            if (min_timeout == -1 || remaining < min_timeout)
                min_timeout = static_cast<int>(remaining);
        }

        return min_timeout;
    }

    /*TODO: Add better command parsing logic */
    bool ReplicationManager::handle(ClientContext &c, ServerConfig &config)
    {
        // Parse the command
        auto command = CommandParser::parseCommand(c);
        c.current_read_position = 0;
        if (command->type == CommandType::INFO)
        {
            handle_info(c, config);
            return true;
        }

        if (command->type == CommandType::REPLCONF)
        {
            handle_replconf(c, config);
            return true;
        }
        if (command->type == CommandType::PSYNC)
        {
            handle_psync(c, config);
            return true;
        }
        if (command->type == CommandType::WAIT)
        {
            handle_wait(c, config);
            return true;
        }
        return false;
    }
    bool ReplicationManager::handle_propagate(ClientContext &c, ServerConfig &config)
    {
        std::cout << "[handle_propagate] slave_clients.size()=" << slave_clients.size() << std::endl;
        if (slave_clients.empty())
            return false;
        c.current_read_position = 0;
        auto command = CommandParser::parseCommand(c);

        std::cout << "[handle_propagate] Command type=" << static_cast<int>(command->type)
                  << " is_write_command=" << command->is_write_command() << std::endl;
        std::cout << "[handle_propagate] to_resp()=" << command->to_resp() << std::endl;

        if (command->is_write_command())
        {
            std::string raw_resp_command = command->to_resp();
            // Track bytes sent for WAIT command
            config.offset += raw_resp_command.size();
            std::cout << "[handle_propagate] Master offset now=" << config.offset << std::endl;
            std::cout << "[handle_propagate] Propagating to " << slave_clients.size() << " slaves" << std::endl;
            auto it = slave_clients.begin();
            while (it != slave_clients.end())
            {
                if (auto slave = it->lock())
                {
                    std::cout << "[handle_propagate] Adding " << raw_resp_command.size()
                              << " bytes to slave fd=" << slave->fd << std::endl;
                    slave->write_buffer.insert(
                        slave->write_buffer.end(),
                        raw_resp_command.begin(),
                        raw_resp_command.end());
                    ++it;
                }
                else
                {
                    it = slave_clients.erase(it);
                }
            }
            return true;
        }

        return false;
    }
    void ReplicationManager::for_each_active_slaves(std::function<void(std::shared_ptr<Client>)> action)
    {
        auto it = slave_clients.begin();
        while (it != slave_clients.end())
        {
            if (auto slave = it->lock())
            {
                action(slave);
                ++it;
            }
            else
            {
                it = slave_clients.erase(it);
            }
        }
    }
}
