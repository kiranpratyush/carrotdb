#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_zcard(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::ZCARD)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        ZcardCommand *zcardCmd = static_cast<ZcardCommand *>(c.command.get());
        const std::string &key = zcardCmd->key;

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
        encode_integer(&c.client->write_buffer, static_cast<long long>(sortedSet->size()));

        c.current_write_position = 0;
    }
}
