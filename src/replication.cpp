#include "replication.h"
#include <iostream>
#include <fstream>
#include <vector>
#include "parser/command-parser.h"

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
{
    void MasterClient::handle_master(bool is_read_event, bool &should_invert)
    {

        handle_handshake(is_read_event, should_invert);
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
                replication_status == ReplicationStatus::HANDSHAKE_SUCCESS;
                is_handshake_completed = true;
                should_invert = false;
            }
            else if (replication_status == ReplicationStatus::HANDSHAKE_SUCCESS)
            {
                should_invert = false;
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

    void ReplicationManager::handle_info(ClientContext c, ServerConfig &config)
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
    void ReplicationManager::handle_replconf(ClientContext c, ServerConfig &config)
    {
        encode_simple_string(&c.client->write_buffer, "OK");
        return;
    }

    void ReplicationManager::handle_psync(ClientContext c, ServerConfig &config)
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
        

        c.client->isslave = true;
        slave_clients.push_back(c.client);
        return;
        
        if (rdb_content.empty())
        {
            return;
        }

        // Mark the client as slave at this moment
        
    }
    bool ReplicationManager::handle(ClientContext c, ServerConfig &config)
    {
        // Parse the command
        auto command = CommandParser::parseCommand(c);
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
        return false;
    }
    bool ReplicationManager::handle_propagate(ClientContext c)
    {
        if (slave_clients.empty())
            return false;
        auto command = CommandParser::parseCommand(c);

        std::cout<<command->to_resp()<<"resp"<<std::endl;

        if (command->is_write_command())
        {   
            std::string raw_resp_command = command->to_resp();
            auto it = slave_clients.begin();
            while (it != slave_clients.end())
            {
                if (auto slave = it->lock())
                {
                    // Append the pre-built string directly
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
