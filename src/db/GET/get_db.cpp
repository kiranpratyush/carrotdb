#include "db/db.h"

namespace REDIS_NAMESPACE
{
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
}
