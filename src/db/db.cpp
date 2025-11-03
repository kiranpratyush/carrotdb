#include "db.h"
#include "../listpack.h"
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
        std::string_view slice{c.client.read_buffer.data() + t.start_pos, t.end_pos - t.start_pos + 1};

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
            handle_lpop(c);

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
                std::string_view optional_arg{c.client.read_buffer.data() + optional_parameter.start_pos, optional_parameter.end_pos - optional_parameter.start_pos + 1};
                if (is_equal(optional_arg, "EX"))
                {
                    ParsedToken parameter_value = Parser::Parse(c);
                    if (parameter_value.type == ParsedToken::Type::BULK_STRING)
                    {
                        std::string_view possible_number{c.client.read_buffer.data() + parameter_value.start_pos, parameter_value.end_pos - parameter_value.start_pos + 1};
                        unsigned long long time_in_seconds;
                        if (convert_positive_string_to_number(possible_number, time_in_seconds))
                        {
                            // store here the item in the db
                            auto steady_now = std::chrono::steady_clock::now();
                            auto steady_expiration_time = steady_now + std::chrono::seconds(time_in_seconds);
                            std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
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
                        std::string_view possible_number{c.client.read_buffer.data() + parameter_value.start_pos, parameter_value.end_pos - parameter_value.start_pos + 1};
                        unsigned long long time_in_milliseconds;
                        if (convert_positive_string_to_number(possible_number, time_in_milliseconds))
                        {
                            // store here the item in the db
                            auto steady_now = std::chrono::steady_clock::now();
                            auto steady_expiration_time = steady_now + std::chrono::milliseconds(time_in_milliseconds);
                            std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
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
            std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
            std::string value{c.client.read_buffer.data() + value_token.start_pos, value_token.end_pos - value_token.start_pos + 1};
            RedisObject redisObj;
            redisObj.type = RedisObjectEncodingType::STRING;
            redisObj.stringPtr = std::make_unique<std::string>(value);
            store[key] = std::move(redisObj);
            c.client.write_buffer.append("+OK\r\n");
            c.current_write_position = 0;
        }
    }

    void DB::handle_push(ClientContext &c, int total_commands, bool is_prepend)
    {
        ParsedToken key_token = Parser::Parse(c);
        total_commands--;
        uint32_t len{};
        while (total_commands > 0)
        {
            ParsedToken value_token = Parser::Parse(c);
            if (key_token.type == ParsedToken::Type::BULK_STRING && value_token.type == ParsedToken::Type::BULK_STRING)
            {
                // Check if the key exists in the store,get the redisObject if exists
                std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
                unsigned char *value_ptr = reinterpret_cast<unsigned char *>(
                    c.client.read_buffer.data() + value_token.start_pos);
                uint32_t value_len = value_token.end_pos - value_token.start_pos + 1;
                auto it = store.find(key);
                if (it == store.end())
                {
                    auto listpackptr = lpNew(LIST_PACK_INITIAL_CAPACITY);
                    if (is_prepend)
                        listpackptr = lpPrepend(std::move(listpackptr), value_ptr, value_len);
                    else
                        listpackptr = lpAppend(std::move(listpackptr), value_ptr, value_len);
                    RedisObject redisObject{RedisObjectEncodingType::LIST_PACK, nullptr, std::move(listpackptr)};
                    store[key] = std::move(redisObject);
                    len = 1;
                }
                else
                {
                    RedisObject &redisObject = it->second;
                    assert(redisObject.type == RedisObjectEncodingType::LIST_PACK);
                    std::unique_ptr<unsigned char[]> listpackPtr{};
                    if (is_prepend)
                        listpackPtr = lpPrepend(std::move(redisObject.listPack), value_ptr, value_len);
                    else
                        listpackPtr = lpAppend(std::move(redisObject.listPack), value_ptr, value_len);
                    len = lpGetTotalNumElements(listpackPtr.get());
                    redisObject.listPack = std::move(listpackPtr);
                }
            }
            total_commands--;
        }
        c.client.write_buffer.append(":" + std::to_string(len) + "\r\n");
        c.current_write_position = 0;
    }

    void DB::handle_lrange(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        ParsedToken start_index_token = Parser::Parse(c);
        ParsedToken end_index_token = Parser::Parse(c);
        std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        std::string start_index_string{c.client.read_buffer.data() + start_index_token.start_pos, start_index_token.end_pos - start_index_token.start_pos + 1};
        std::string end_index_string{c.client.read_buffer.data() + end_index_token.start_pos, end_index_token.end_pos - end_index_token.start_pos + 1};

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
            c.client.write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }

        auto it = store.find(key);
        if (it == store.end())
        {
            c.client.write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }

        // Verify it's a list type
        const RedisObject &redisObj = it->second;
        if (redisObj.type != RedisObjectEncodingType::LIST_PACK || !redisObj.listPack)
        {
            c.client.write_buffer.append("*0\r\n");
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
            c.client.write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }
        if (start_index >= (long long)list_length)
        {
            c.client.write_buffer.append("*0\r\n");
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
        c.client.write_buffer.append("*" + std::to_string(result.size()) + "\r\n");
        for (const auto &item : result)
        {
            c.client.write_buffer.append("$" + std::to_string(item.length()) + "\r\n");
            c.client.write_buffer.append(item + "\r\n");
        }
        c.current_write_position = 0;
    }

    void DB::handle_llen(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        auto it = store.find(key);
        if (it == store.end())
        {
            c.client.write_buffer.append(":0\r\n");
            c.current_write_position = 0;
            return;
        }
        const RedisObject &redisObj = it->second;
        if (redisObj.type != RedisObjectEncodingType::LIST_PACK || !redisObj.listPack)
        {
            c.client.write_buffer.append(":0\r\n");
            c.current_write_position = 0;
            return;
        }
        unsigned long list_length = lpLength(redisObj.listPack.get());
        c.client.write_buffer.append(":" + std::to_string(list_length) + "\r\n");
        c.current_write_position = 0;
    }

    void DB::handle_lpop(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        auto it = store.find(key);
        if (it == store.end())
        {
            c.client.write_buffer.append("$-1\r\n");
            c.current_write_position = 0;
            return;
        }
        RedisObject &redisObj = it->second; // Remove const - we need to modify it
        if (redisObj.type != RedisObjectEncodingType::LIST_PACK || !redisObj.listPack)
        {
            c.client.write_buffer.append("$-1\r\n");
            c.current_write_position = 0;
            return;
        }
        unsigned char *p = lpFirst(redisObj.listPack.get());
        if (!p)
        {
            c.client.write_buffer.append("$-1\r\n");
            c.current_write_position = 0;
            return;
        }

        // Get the value before deleting
        int64_t value_length;
        unsigned char *valuePtr = lpGetWithSize(p, &value_length, nullptr);
        std::string result;

        if (valuePtr) // It's a string
        {
            result = std::string(reinterpret_cast<char *>(valuePtr), value_length);
        }
        else // It's an integer
        {
            result = std::to_string(value_length);
        }

        // Delete the first element (pass p, not valuePtr!)
        auto listpackPtr = lpDelete(std::move(redisObj.listPack), p, nullptr);

        // Store the modified listpack back
        redisObj.listPack = std::move(listpackPtr);

        // Build proper RESP bulk string response
        c.client.write_buffer.append("$" + std::to_string(result.length()) + "\r\n" + result + "\r\n");
        c.current_write_position = 0;
    }

    void DB::handle_get(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type == ParsedToken::Type::BULK_STRING)
        {
            std::string key{c.client.read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
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
                    c.client.write_buffer.append("$" + std::to_string(redisObject.stringPtr->length()) + "\r\n" + *redisObject.stringPtr + "\r\n");
                }
            }
            else
            {
                c.client.write_buffer.append("$-1\r\n");
            }
            c.current_write_position = 0;
        }
    }

    void DB::handle_ping(ClientContext &c)
    {
        c.client.write_buffer.append("+PONG\r\n");
        c.current_write_position = 0;
    }
    void DB::handle_echo(ClientContext &c)
    {
        ParsedToken t = Parser::Parse(c);
        if (t.type == ParsedToken::Type::BULK_STRING)
        {
            std::string arg{c.client.read_buffer.data() + t.start_pos, t.end_pos - t.start_pos + 1};
            c.client.write_buffer.append("$" + std::to_string(arg.length()) + "\r\n" + arg + "\r\n");
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

}