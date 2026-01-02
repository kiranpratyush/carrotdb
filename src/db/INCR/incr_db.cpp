#include "db/db.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_incr(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        std::string key{c.client->read_buffer.data() + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        auto it = store.find(key);
        if (it == store.end())
        {
            RedisObject redisObj;
            redisObj.type = RedisObjectEncodingType::STRING;
            redisObj.stringPtr = std::make_unique<std::string>("1");
            store[key] = std::move(redisObj);
            c.client->write_buffer.append(":1\r\n");
        }
        else
        {
            RedisObject object = std::move(it->second);
            bool is_okay = false;
            if (object.type == RedisObjectEncodingType::STRING)
            {
                int64_t value;
                is_okay = convert_string_to_number(object.stringPtr.get()->data(), &value);
                if (is_okay)
                {
                    value++;
                    c.client->write_buffer.append(":" + std::to_string(value) + "\r\n");

                    object.stringPtr = std::make_unique<std::string>(std::to_string(value));
                    store[key] = std::move(object);
                }
            }
            if (!is_okay)
            {
                c.client->write_buffer.append("-ERR value is not an integer or out of range\r\n");
            }
        }
        c.current_write_position = 0;
    }
}