#include "db.h"
#include "parser/command-parser.h"
#include "../listpack.h"
#include <algorithm>
#include <cassert>

namespace REDIS_NAMESPACE
{
    void DB::execute(ClientContext &c)
    {
        ParsedToken t = Parser::Parse(c);

        if (t.type != ParsedToken::Type::ARRAY)
        {
            return;
        }
        auto total_commands = t.length;
        
        // Parse command using CommandParser
        Command cmd = CommandParser::parseCommand(c, total_commands);
        
        // Execute based on command type
        switch (cmd.type)
        {
            case CommandType::PING:
                handle_ping(c);
                break;
            case CommandType::ECHO:
                handle_echo(c, static_cast<const EchoCommand&>(cmd));
                break;
            case CommandType::SET:
                handle_set(c, static_cast<const SetCommand&>(cmd));
                break;
            case CommandType::GET:
                handle_get_or_type(c, static_cast<const GetCommand&>(cmd), false);
                break;
            case CommandType::TYPE:
                handle_get_or_type(c, static_cast<const TypeCommand&>(cmd), true);
                break;
            case CommandType::INCR:
                handle_incr(c, static_cast<const IncrCommand&>(cmd));
                break;
            case CommandType::RPUSH:
                handle_push(c, static_cast<const RpushCommand&>(cmd), false);
                break;
            case CommandType::LPUSH:
                handle_push(c, static_cast<const LpushCommand&>(cmd), true);
                break;
            case CommandType::LRANGE:
                handle_lrange(c, static_cast<const LrangeCommand&>(cmd));
                break;
            case CommandType::LLEN:
                handle_llen(c, static_cast<const LlenCommand&>(cmd));
                break;
            case CommandType::LPOP:
                handle_lpop(c, static_cast<const LpopCommand&>(cmd));
                break;
            case CommandType::BLPOP:
                handle_blpop(c, static_cast<const BlpopCommand&>(cmd));
                break;
            case CommandType::XADD:
                handle_xadd(c, static_cast<const XaddCommand&>(cmd));
                break;
            case CommandType::XRANGE:
                handle_xrange(c, static_cast<const XrangeCommand&>(cmd));
                break;
            case CommandType::XREAD:
                handle_xread(c, static_cast<const XreadCommand&>(cmd));
                break;
            default:
                handle_ping(c);
                break;
        }
    }

    void DB::handle_ping(ClientContext &c)
    {
        encode_simple_string(&c.client->write_buffer, "PONG");
        c.current_write_position = 0;
    }

    void DB::handle_echo(ClientContext &c, const EchoCommand &cmd)
    {
        encode_bulk_string(&c.client->write_buffer, cmd.message);
        c.current_write_position = 0;
    }

    void DB::signal_key_ready(const std::string &key, ClientContext &context)
    {
        auto it = blocked_keys.find(key);
        if (it != blocked_keys.end() && !it->second.empty())
        {
            BlockedClient blocked_client = it->second.front();
            it->second.pop();

            // If queue is now empty, remove the key entry
            if (it->second.empty())
            {
                blocked_keys.erase(it);
            }

            auto client_ptr = blocked_client.client.lock();
            if (client_ptr)
            {
                // Store the unblocked client fd so server can make it ready for write
                context.unblocked_client_fd = blocked_client.client_fd;
            }
        }
    }

    std::vector<int> DB::check_and_expire_blocked_clients()
    {
        std::vector<int> expired_fds;
        std::vector<std::string> keys_to_remove;

        // Handle BLPOP blocked clients
        for (auto &[key, client_queue] : blocked_keys)
        {
            std::queue<BlockedClient> new_queue;

            while (!client_queue.empty())
            {
                BlockedClient blocked_client = client_queue.front();
                client_queue.pop();

                if (blocked_client.is_expired())
                {
                    // Client timed out - send null response
                    auto client_ptr = blocked_client.client.lock();
                    if (client_ptr)
                    {
                        client_ptr->write_buffer.append("*-1\r\n"); // Null array for timeout
                        expired_fds.push_back(blocked_client.client_fd);
                    }
                }
                else
                {
                    // Still valid, keep in queue
                    new_queue.push(blocked_client);
                }
            }

            if (new_queue.empty())
            {
                keys_to_remove.push_back(key);
            }
            else
            {
                client_queue = std::move(new_queue);
            }
        }
        for (const auto &key : keys_to_remove)
        {
            blocked_keys.erase(key);
        }

        // Handle XREAD blocked clients
        keys_to_remove.clear();
        for (auto &[key, client_queue] : blocked_xread_keys)
        {
            std::queue<BlockedXreadClient> new_queue;

            while (!client_queue.empty())
            {
                BlockedXreadClient blocked_client = client_queue.front();
                client_queue.pop();

                if (blocked_client.is_expired())
                {
                    // Client timed out - send null response
                    auto client_ptr = blocked_client.client.lock();
                    if (client_ptr)
                    {
                        client_ptr->write_buffer.append("*-1\r\n"); // Null array for timeout
                        expired_fds.push_back(blocked_client.client_fd);
                    }
                }
                else
                {
                    // Still valid, keep in queue
                    new_queue.push(blocked_client);
                }
            }

            if (new_queue.empty())
            {
                keys_to_remove.push_back(key);
            }
            else
            {
                client_queue = std::move(new_queue);
            }
        }
        for (const auto &key : keys_to_remove)
        {
            blocked_xread_keys.erase(key);
        }

        return expired_fds;
    }

}