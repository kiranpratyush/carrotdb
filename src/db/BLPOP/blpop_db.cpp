#include "db/db.h"
#include "listpack.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_blpop(ClientContext &context, int total_commands)
    {
        auto client = context.client;
        auto *read_buffer = client->read_buffer.data();
        uint32_t expiration_time = 0;

        ParsedToken key_token = Parser::Parse(context);
        std::string key{read_buffer + key_token.start_pos, key_token.end_pos - key_token.start_pos + 1};
        total_commands--;

        if (total_commands > 0)
        {
            ParsedToken expire_timeout_token = Parser::Parse(context);
            std::string expire_time_out_string{read_buffer + expire_timeout_token.start_pos,
                                               expire_timeout_token.end_pos - expire_timeout_token.start_pos + 1};
            try
            {
                expiration_time = std::stoll(expire_time_out_string);
            }
            catch (...)
            {
                expiration_time = 0;
            }
        }

        auto it = store.find(key);
        if (it == store.end())
        {
            context.isBlocked = true;
            blocked_keys[key].push({context.client, context.client_fd});
            return;
        }

        RedisObject &redisObject = it->second;
        if (redisObject.type != RedisObjectEncodingType::LIST_PACK || !redisObject.listPack)
        {
            client->write_buffer.append("$-1\r\n");
            context.current_write_position = 0;
            return;
        }

        unsigned char *p = lpFirst(redisObject.listPack.get());
        if (!p)
        {
            context.isBlocked = true;
            blocked_keys[key].push({context.client, context.client_fd});
            return;
        }

        int64_t value_length;
        unsigned char *valuePtr = lpGetWithSize(p, &value_length, nullptr);

        std::string value;
        if (valuePtr) // It's a string
        {
            value = std::string(reinterpret_cast<char *>(valuePtr), value_length);
        }
        else // It's an integer
        {
            value = std::to_string(value_length);
        }

        auto listpackPtr = lpDelete(std::move(redisObject.listPack), p, nullptr);
        redisObject.listPack = std::move(listpackPtr);

        write_blpop_response(client, key, value);
        context.current_write_position = 0;
    }
}
