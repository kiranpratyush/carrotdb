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
            handle_set(c);
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

    void DB::handle_set(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        ParsedToken value_token = Parser::Parse(c);
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