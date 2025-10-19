#include "parser.h"

namespace REDIS_NAMESPACE
{

    ParsedToken Parser::Parse(ClientContext &c)
    {
        size_t buffer_size = c.client.read_buffer.size();

        // if the current character starts with * , it is an array
        if (c.current_read_position < buffer_size && c.client.read_buffer[c.current_read_position] == '*')
        {
            c.current_read_position += 1;
            unsigned long length{};
            if (calculate_length(c, length))
            {
                return ParsedToken{ParsedToken::Type::ARRAY, 0, 0, length};
            }
        }
        else if (c.current_read_position < buffer_size && c.client.read_buffer[c.current_read_position] == '$')
        {
            c.current_read_position += 1;
            unsigned long length{};
            if (calculate_length(c, length))
            {
                // calculate the string
                unsigned long start_pos = c.current_read_position;
                while (c.current_read_position < buffer_size && c.client.read_buffer[c.current_read_position] != '\r')
                {
                    c.current_read_position++;
                }
                unsigned long end_pos = c.current_read_position - 1;
                c.current_read_position += 2; // skip
                return ParsedToken{ParsedToken::Type::BULK_STRING, start_pos, end_pos, length};
            }
        }
        return ParsedToken{ParsedToken::Type::UNKNOWN, 0, 0, 0};
    }

    bool Parser::calculate_length(ClientContext &c, unsigned long &length)
    {
        while (c.current_read_position < c.client.read_buffer.size() &&
               c.client.read_buffer[c.current_read_position] != '\r')
        {
            char x = c.client.read_buffer[c.current_read_position];
            if (isdigit(x))
            {
                length = (length * 10) + (x - '0');
            }
            else
            {
                return false;
            }
            c.current_read_position += 1;
        }
        c.current_read_position += 2;
        return true;
    }
}