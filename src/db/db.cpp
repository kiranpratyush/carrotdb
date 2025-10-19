#include "db.h"

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
        {
            handle_echo(c);
        }
        else if (is_equal(slice, "PING"))
        {
            handle_ping(c);
        }
        else if (is_equal(slice, "SET"))
        {
            handle_set(c, total_commands - 1);
        }
        else if (is_equal(slice, "GET"))
        {
            handle_get(c);
        }
        else
        {
            handle_ping(c);
        }
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
            store[key] = value;
            c.client.write_buffer.append("+OK\r\n");
            c.current_write_position = 0;
        }
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
                const std::string &value = it->second;
                c.client.write_buffer.append("$" + std::to_string(value.length()) + "\r\n" + value + "\r\n");
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