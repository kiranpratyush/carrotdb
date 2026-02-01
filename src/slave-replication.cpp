#include "slave-replication.h"
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include "parser/command-parser.h"
#include "networking.h"

using namespace REDIS_NAMESPACE;

namespace SLAVE_REPLICATION_NAMESPACE
{
    void SlaveReplicationClient::set_db_and_config(REDIS_NAMESPACE::DB *database, ServerConfig *server_config)
    {
        db = database;
        config = server_config;
    }

    void SlaveReplicationClient::process_read_buffer()
    {
        std::cout << "[process_read_buffer] handshake_completed=" << is_handshake_completed << std::endl;

        if (!is_handshake_completed)
        {
            handle_handshake_read();
            if (is_handshake_completed && !read_buffer.empty())
            {
                std::cout << "[process_read_buffer] Handshake done, processing remaining buffer" << std::endl;
                process_commands();
            }
        }
        else
        {
            std::cout << "[process_read_buffer] Processing commands" << std::endl;
            process_commands();
        }
    }

    void SlaveReplicationClient::process_commands()
    {
        if (read_buffer.empty())
        {
            std::cout << "[process_commands] Buffer empty" << std::endl;
            return;
        }

        while (!read_buffer.empty() && db && config)
        {
            auto client_context = ClientContext{std::shared_ptr<Client>(this, [](Client *) {}), fd};
            client_context.command = CommandParser::parseCommand(client_context);

            if (client_context.command->type == CommandType::UNKNOWN)
            {
                std::cout << "[process_commands] buffer" << client_context.client->read_buffer << std::endl;
                break;
            }

            if (client_context.command->type == CommandType::REPLCONF)
            {
                auto *replconf_cmd = static_cast<ReplConfCommand *>(client_context.command.get());
                if (replconf_cmd->subcommand == ReplConfType::GETACK)
                {
                    std::cout << "[process_commands] GETACK, ACK " << bytes_processed << std::endl;
                    write_buffer.clear();
                    encode_array_header(&write_buffer, 3);
                    encode_bulk_string(&write_buffer, "REPLCONF");
                    encode_bulk_string(&write_buffer, "ACK");
                    encode_bulk_string(&write_buffer, std::to_string(bytes_processed));
                    bytes_processed += client_context.command->bytes_processed;
                    continue;
                }
            }
            db->execute(client_context, config);
            bytes_processed += client_context.command->bytes_processed;
            write_buffer.clear();
            std::cout << "[process_commands] bytes_processed=" << bytes_processed << std::endl;
        }
    }
    void SlaveReplicationClient::handle_handshake_read()
    {
        std::string response;
        bool status = Parser::parse_simple_string(read_buffer, response);
        if (!status)
            return;

        if (replication_status == ReplicationStatus::PING_SENT && is_equal(response, "PONG"))
        {
            replication_status = ReplicationStatus::PING_SENT_SUCCESS;
            read_buffer.clear();
            prepare_handshake_write();
        }
        else if (replication_status == ReplicationStatus::REPLCONF_PORT_SENT && is_equal(response, "OK"))
        {
            replication_status = ReplicationStatus::REPLCONF_PORT_SUCCESS;
            read_buffer.clear();
            prepare_handshake_write();
        }
        else if (replication_status == ReplicationStatus::REPLCONF_CAPA_SENT && is_equal(response, "OK"))
        {
            replication_status = ReplicationStatus::REPLCONF_CAPA_SUCCESS;
            read_buffer.clear();
            prepare_handshake_write();
        }
        else if (replication_status == ReplicationStatus::PSYNC_SENT)
        {
            // First, clean up the FULLRESYNC response
            size_t fullresync_end = read_buffer.find("\r\n");
            if (fullresync_end == std::string::npos)
            {
                // Incomplete FULLRESYNC response, wait for more data
                return;
            }
            std::string fullresync_response = read_buffer.substr(0, fullresync_end);
            std::cout << "FULLRESYNC response: " << fullresync_response << std::endl;
            read_buffer.erase(0, fullresync_end + 2);

            // Then, handle the RDB if present
            if (!read_buffer.empty() && read_buffer[0] == '$')
            {
                std::cout << "RDB received, skipping..." << std::endl;
                size_t length_end = read_buffer.find("\r\n", 1);
                if (length_end == std::string::npos)
                {
                    // Incomplete RDB header, wait for more data
                    return;
                }
                std::string length_str = read_buffer.substr(1, length_end - 1);
                unsigned long long rdb_length = 0;
                if (convert_positive_string_to_number(length_str, rdb_length))
                {
                    size_t rdb_end = length_end + 2 + rdb_length;
                    if (rdb_end > read_buffer.size())
                    {
                        // Incomplete RDB data, wait for more data
                        return;
                    }
                    read_buffer.erase(0, rdb_end);
                    std::cout << "RDB skipped, remaining: " << read_buffer.size() << " bytes" << std::endl;
                }
            }

            // Mark handshake as completed, remaining commands stay in buffer
            replication_status = ReplicationStatus::HANDSHAKE_SUCCESS;
            is_handshake_completed = true;
            return;
        }
        read_buffer.clear();
    }

    void SlaveReplicationClient::prepare_handshake_write()
    {
        if (replication_status == ReplicationStatus::IDLE)
        {
            encode_array_header(&write_buffer, 1);
            encode_bulk_string(&write_buffer, "PING");
            replication_status = ReplicationStatus::PING_SENT;
        }
        else if (replication_status == ReplicationStatus::PING_SENT_SUCCESS)
        {
            encode_array_header(&write_buffer, 3);
            encode_bulk_string(&write_buffer, "REPLCONF");
            encode_bulk_string(&write_buffer, "listening-port");
            encode_bulk_string(&write_buffer, std::to_string(port));
            replication_status = ReplicationStatus::REPLCONF_PORT_SENT;
        }
        else if (replication_status == ReplicationStatus::REPLCONF_PORT_SUCCESS)
        {
            encode_array_header(&write_buffer, 3);
            encode_bulk_string(&write_buffer, "REPLCONF");
            encode_bulk_string(&write_buffer, "capa");
            encode_bulk_string(&write_buffer, "psync2");
            replication_status = ReplicationStatus::REPLCONF_CAPA_SENT;
        }
        else if (replication_status == ReplicationStatus::REPLCONF_CAPA_SUCCESS)
        {
            encode_array_header(&write_buffer, 3);
            encode_bulk_string(&write_buffer, "PSYNC");
            encode_bulk_string(&write_buffer, repl_id);
            encode_bulk_string(&write_buffer, std::to_string(offset));
            replication_status = ReplicationStatus::PSYNC_SENT;
        }
    }
}
