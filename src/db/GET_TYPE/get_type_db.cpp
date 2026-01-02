#include "db/db.h"
#include "utils/utils.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_get_or_type(ClientContext &c, const GetCommand &cmd, bool isTypeCommand)
    {
        const std::string &key = cmd.key;
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
                    if (isTypeCommand)
                    {
                        encode_simple_string(&c.client->write_buffer, "string");
                    }
                    else
                    {
                        encode_bulk_string(&c.client->write_buffer, *redisObject.stringPtr);
                    }
                }
                else if (redisObject.type == RedisObjectEncodingType::STREAM)
                {
                    if (isTypeCommand)
                    {
                        encode_simple_string(&c.client->write_buffer, "stream");
                    }
                }
            }
            else
            {
                if (isTypeCommand)
                    encode_simple_string(&c.client->write_buffer, "none");
                else
                    encode_null_bulk_string(&c.client->write_buffer);
            }
            c.current_write_position = 0;
    }

    void DB::handle_get_or_type(ClientContext &c, const TypeCommand &cmd, bool isTypeCommand)
    {
        const std::string &key = cmd.key;
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
                if (isTypeCommand)
                {
                    encode_simple_string(&c.client->write_buffer, "string");
                }
                else
                {
                    encode_bulk_string(&c.client->write_buffer, *redisObject.stringPtr);
                }
            }
            else if (redisObject.type == RedisObjectEncodingType::STREAM)
            {
                if (isTypeCommand)
                {
                    encode_simple_string(&c.client->write_buffer, "stream");
                }
            }
        }
        else
        {
            if (isTypeCommand)
                encode_simple_string(&c.client->write_buffer, "none");
            else
                encode_null_bulk_string(&c.client->write_buffer);
        }
        c.current_write_position = 0;
    }
}
