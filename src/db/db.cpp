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
        else
        {
            handle_ping(c);
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