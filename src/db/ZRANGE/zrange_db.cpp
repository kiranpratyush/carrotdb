#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_zrange(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::ZRANGE)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        ZrangeCommand *zrangeCmd = static_cast<ZrangeCommand *>(c.command.get());
        const std::string &key = zrangeCmd->key;

        auto it = store.find(key);

        if (it == store.end())
        {
            encode_empty_array(&c.client->write_buffer);
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
        std::vector<std::string> members = sortedSet->range(zrangeCmd->start, zrangeCmd->stop);

        encode_array_header(&c.client->write_buffer, members.size());
        for (const auto &member : members)
        {
            encode_bulk_string(&c.client->write_buffer, member);
        }

        c.current_write_position = 0;
    }
}
