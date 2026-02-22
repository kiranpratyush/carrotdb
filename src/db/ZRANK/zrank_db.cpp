#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_zrank(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::ZRANK)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        ZrankCommand *zrankCmd = static_cast<ZrankCommand *>(c.command.get());
        const std::string &key = zrankCmd->key;
        const std::string &member = zrankCmd->member;

        auto it = store.find(key);

        if (it == store.end())
        {
            encode_null_bulk_string(&c.client->write_buffer);
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
        std::optional<size_t> rank = sortedSet->rank(member);

        if (rank.has_value())
        {
            encode_integer(&c.client->write_buffer, static_cast<long long>(rank.value()));
        }
        else
        {
            encode_null_bulk_string(&c.client->write_buffer);
        }

        c.current_write_position = 0;
    }
}
