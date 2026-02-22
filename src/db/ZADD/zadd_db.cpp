#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_zadd(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::ZADD)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        ZaddCommand *zaddCmd = static_cast<ZaddCommand *>(c.command.get());
        const std::string &key = zaddCmd->key;

        auto it = store.find(key);

        if (it == store.end())
        {
            RedisObject redisObject{};
            redisObject.type = RedisObjectEncodingType::SORTED_SET;
            redisObject.sortedSetPtr = std::make_unique<SortedSet>();
            store[key] = std::move(redisObject);
            it = store.find(key);
        }

        if (it->second.type != RedisObjectEncodingType::SORTED_SET)
        {
            encode_error(&c.client->write_buffer, "WRONGTYPE Operation against a key holding the wrong kind of value");
            c.current_write_position = 0;
            return;
        }

        size_t added = 0;
        SortedSet *sortedSet = it->second.sortedSetPtr.get();

        for (const auto &sm : zaddCmd->score_members)
        {
            sortedSet->insert(sm.second, sm.first);
            added++;
        }

        encode_integer(&c.client->write_buffer, static_cast<long long>(added));
        c.current_write_position = 0;
    }
}
