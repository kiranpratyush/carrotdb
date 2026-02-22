#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_zrem(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::ZREM)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        ZremCommand *zremCmd = static_cast<ZremCommand *>(c.command.get());
        const std::string &key = zremCmd->key;

        auto it = store.find(key);

        if (it == store.end())
        {
            encode_integer(&c.client->write_buffer, 0);
            c.current_write_position = 0;
            return;
        }

        if (it->second.type != RedisObjectEncodingType::SORTED_SET)
        {
            encode_error(&c.client->write_buffer, "WRONGTYPE Operation against a key holding the wrong kind of value");
            c.current_write_position = 0;
            return;
        }

        SortedSet *sortedSet = it->second.sortedSetPtr.get();
        size_t removed = 0;

        for (const auto &member : zremCmd->members)
        {
            if (sortedSet->remove(member))
            {
                removed++;
            }
        }

        encode_integer(&c.client->write_buffer, static_cast<long long>(removed));

        c.current_write_position = 0;
    }
}
