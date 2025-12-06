#include "db.h"
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
        t = Parser::Parse(c);
        std::string_view slice{c.client->read_buffer.data() + t.start_pos, t.end_pos - t.start_pos + 1};

        if (is_equal(slice, "ECHO"))
            handle_echo(c);
        else if (is_equal(slice, "PING"))
            handle_ping(c);

        else if (is_equal(slice, "SET"))
            handle_set(c, total_commands - 1);

        else if (is_equal(slice, "GET"))
            handle_get(c);

        else if (is_equal(slice, "RPUSH"))
            handle_push(c, total_commands - 1, false);

        else if (is_equal(slice, "LPUSH"))
            handle_push(c, total_commands - 1, true);

        else if (is_equal(slice, "LRANGE"))
            handle_lrange(c);

        else if (is_equal(slice, "LLEN"))
            handle_llen(c);

        else if (is_equal(slice, "LPOP"))
            handle_lpop(c, total_commands - 1);

        else if (is_equal(slice, "BLPOP"))
            handle_blpop(c, total_commands - 1);
        else
            handle_ping(c);
    }

    void DB::handle_set(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        ParsedToken value_token = Parser::Parse(c);
        total_commands -= 2;
        while (total_commands > 0)
        {
            ParsedToken optional_parameter = Parser::Parse(c);
            if (optional_parameter.type == ParsedToken::Type::BULK_STRING)
            {
                std::string_view optional_arg{c.client->read_buffer.data() + optional_parameter.start_pos, optional_parameter.end_pos - optional_parameter.start_pos + 1};
                if (is_equal(optional_arg, "EX"))
                {
                    ParsedToken parameter_value = Parser::Parse(c);
                    if (parameter_value.type == ParsedToken::Type::BULK_STRING)
                    {
                        std::string_view possible_number{c.client->read_buffer.data() + parameter_value.start_pos, parameter_value.end_pos - parameter_value.start_pos + 1};
                        unsigned long long time_in_seconds;
                        if (convert_positive_string_to_number(possible_number, time_in_seconds))
                        {
                            // store here the item in the db
                            auto steady_now = std::chrono::steady_clock::now();
                            auto steady_expiration_time = steady_now + std::chrono::seconds(time_in_seconds);
                            std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
                            expiration[key] = steady_expiration_time;
                            // store the steady time point in the db
                        }
                    }
                }
                else if (is_equal(optional_arg, "PX"))
                {
                    ParsedToken parameter_value = Parser::Parse(c);
                    if (parameter_value.type == ParsedToken::Type::BULK_STRING)
                    {
                        std::string_view possible_number{c.client->read_buffer.data() + parameter_value.start_pos, parameter_value.end_pos - parameter_value.start_pos + 1};
                        unsigned long long time_in_milliseconds;
                        if (convert_positive_string_to_number(possible_number, time_in_milliseconds))
                        {
                            // store here the item in the db
                            auto steady_now = std::chrono::steady_clock::now();
                            auto steady_expiration_time = steady_now + std::chrono::milliseconds(time_in_milliseconds);
                            std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
                            expiration[key] = steady_expiration_time;
                            // store the steady time point in the db
                        }
                    }
                }
                total_commands--;
            }
            total_commands--;
        }
        // Skip any additional arguments for simplicity
        if (key_token.type == ParsedToken::Type::BULK_STRING &&
            value_token.type == ParsedToken::Type::BULK_STRING)
        {
            std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
            std::string value{c.client->read_buffer.data() + value_token.start_pos, value_token.end_pos - value_token.start_pos + 1};
            RedisObject redisObj;
            redisObj.type = RedisObjectEncodingType::STRING;
            redisObj.stringPtr = std::make_unique<std::string>(value);
            store[key] = std::move(redisObj);
            c.client->write_buffer.append("+OK\r\n");
            c.current_write_position = 0;
        }
    }

    void DB::handle_lrange(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        ParsedToken start_index_token = Parser::Parse(c);
        ParsedToken end_index_token = Parser::Parse(c);
        std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        std::string start_index_string{c.client->read_buffer.data() + start_index_token.start_pos, start_index_token.end_pos - start_index_token.start_pos + 1};
        std::string end_index_string{c.client->read_buffer.data() + end_index_token.start_pos, end_index_token.end_pos - end_index_token.start_pos + 1};

        // Parse as signed integers to support negative indices
        long long start_index;
        long long end_index;
        std::vector<std::string> result{};

        // Try to parse as signed integers (supports negative values like -1)
        try
        {
            start_index = std::stoll(start_index_string);
            end_index = std::stoll(end_index_string);
        }
        catch (...)
        {
            c.client->write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }

        auto it = store.find(key);
        if (it == store.end())
        {
            c.client->write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }

        // Verify it's a list type
        const RedisObject &redisObj = it->second;
        if (redisObj.type != RedisObjectEncodingType::LIST_PACK || !redisObj.listPack)
        {
            c.client->write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }

        // Get list length for bounds checking
        unsigned long list_length = lpLength(redisObj.listPack.get());

        // Convert negative indices to positive (Redis semantics)
        if (start_index < 0)
            start_index = list_length + start_index;
        if (end_index < 0)
            end_index = list_length + end_index;

        // Clamp to valid range
        if (start_index < 0)
            start_index = 0;
        if (end_index < 0)
        {
            c.client->write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }
        if (start_index >= (long long)list_length)
        {
            c.client->write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }
        if (end_index >= (long long)list_length)
            end_index = list_length - 1;

        // Collect results
        for (long long index = start_index; index <= end_index; index++)
        {
            unsigned char *p = lpSeek(redisObj.listPack.get(), index);
            if (!p)
                break; // Out of range

            int64_t value_length;
            unsigned char *valuePtr = lpGetWithSize(p, &value_length, nullptr);
            if (valuePtr) // It's a string
            {
                result.push_back(std::string(reinterpret_cast<char *>(valuePtr), value_length));
            }
            else // It's an integer
            {
                result.push_back(std::to_string(value_length));
            }
        }

        // Build RESP array response
        c.client->write_buffer.append("*" + std::to_string(result.size()) + "\r\n");
        for (const auto &item : result)
        {
            c.client->write_buffer.append("$" + std::to_string(item.length()) + "\r\n");
            c.client->write_buffer.append(item + "\r\n");
        }
        c.current_write_position = 0;
    }

    void DB::handle_llen(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        auto it = store.find(key);
        if (it == store.end())
        {
            c.client->write_buffer.append(":0\r\n");
            c.current_write_position = 0;
            return;
        }
        const RedisObject &redisObj = it->second;
        if (redisObj.type != RedisObjectEncodingType::LIST_PACK || !redisObj.listPack)
        {
            c.client->write_buffer.append(":0\r\n");
            c.current_write_position = 0;
            return;
        }
        unsigned long list_length = lpLength(redisObj.listPack.get());
        c.client->write_buffer.append(":" + std::to_string(list_length) + "\r\n");
        c.current_write_position = 0;
    }

    void DB::handle_lpop(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        std::vector<std::string> result{};
        uint32_t total_count = 1;
        total_commands--;
        if (total_commands > 0)
        {
            ParsedToken count_token = Parser::Parse(c);
            std::string count_token_string{c.client->read_buffer.data() + count_token.start_pos, count_token.end_pos - count_token.start_pos + 1};
            try
            {
                total_count = std::stoll(count_token_string);
            }
            catch (...)
            {
                total_count = 1;
            }
        }
        auto it = store.find(key);
        if (it == store.end())
        {
            c.client->write_buffer.append("$-1\r\n");
            c.current_write_position = 0;
            return;
        }
        RedisObject &redisObj = it->second;
        if (redisObj.type != RedisObjectEncodingType::LIST_PACK || !redisObj.listPack)
        {
            c.client->write_buffer.append("$-1\r\n");
            c.current_write_position = 0;
            return;
        }
        while (total_count > 0)
        {
            unsigned char *p = lpFirst(redisObj.listPack.get());
            if (!p)
            {
                // List is empty, break out
                break;
            }

            // Get the value before deleting
            int64_t value_length;
            unsigned char *valuePtr = lpGetWithSize(p, &value_length, nullptr);

            if (valuePtr) // It's a string
            {
                result.push_back(std::string(reinterpret_cast<char *>(valuePtr), value_length));
            }
            else // It's an integer
            {
                result.push_back(std::to_string(value_length));
            }

            // Delete the first element (pass p, not valuePtr!)
            auto listpackPtr = lpDelete(std::move(redisObj.listPack), p, nullptr);

            // Store the modified listpack back
            redisObj.listPack = std::move(listpackPtr);

            total_count--;
        }
        if (result.size() == 0)
        {
            c.client->write_buffer.append("$-1\r\n");
            c.current_write_position = 0;
            return;
        }
        else if (result.size() == 1)
        {
            // Single result - encode as bulk string
            c.client->write_buffer.append("$" + std::to_string(result[0].length()) + "\r\n");
            c.client->write_buffer.append(result[0] + "\r\n");
            c.current_write_position = 0;
        }
        else
        {
            // Multiple results - encode as array
            c.client->write_buffer.append("*" + std::to_string(result.size()) + "\r\n");
            for (const auto &item : result)
            {
                c.client->write_buffer.append("$" + std::to_string(item.length()) + "\r\n");
                c.client->write_buffer.append(item + "\r\n");
            }
            c.current_write_position = 0;
        }
    }

    void DB::handle_get(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type == ParsedToken::Type::BULK_STRING)
        {
            std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
            auto it = store.find(key);
            // find if the key has expired
            auto exp_it = expiration.find(key);
            if (exp_it != expiration.end())
            {
                auto steady_now = std::chrono::steady_clock::now();
                if (steady_now >= exp_it->second)
                {
                    // key has expired
                    store.erase(it);
                    expiration.erase(exp_it);
                    it = store.end();
                }
            }
            if (it != store.end())
            {
                const RedisObject &redisObject = it->second;
                if (redisObject.type == RedisObjectEncodingType::STRING)
                {
                    c.client->write_buffer.append("$" + std::to_string(redisObject.stringPtr->length()) + "\r\n" + *redisObject.stringPtr + "\r\n");
                }
            }
            else
            {
                c.client->write_buffer.append("$-1\r\n");
            }
            c.current_write_position = 0;
        }
    }

    void DB::handle_ping(ClientContext &c)
    {
        c.client->write_buffer.append("+PONG\r\n");
        c.current_write_position = 0;
    }
    void DB::handle_echo(ClientContext &c)
    {
        ParsedToken t = Parser::Parse(c);
        if (t.type == ParsedToken::Type::BULK_STRING)
        {
            std::string arg{c.client->read_buffer.data() + t.start_pos, t.end_pos - t.start_pos + 1};
            c.client->write_buffer.append("$" + std::to_string(arg.length()) + "\r\n" + arg + "\r\n");
            c.current_write_position = 0;
        }
    }

    bool DB::is_equal(std::string_view s1, std::string_view s2)
    {
        if (s1.size() != s2.size())
        {
            return false;
        }
        for (size_t i = 0; i < s1.size(); i++)
        {
            unsigned char x = static_cast<unsigned char>(s1[i]);
            unsigned char y = static_cast<unsigned char>(s2[i]);
            if (tolower(x) != tolower(y))
            {
                return false;
            }
        }
        return true;
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

        // Remove keys with no blocked clients
        for (const auto &key : keys_to_remove)
        {
            blocked_keys.erase(key);
        }

        return expired_fds;
    }

}