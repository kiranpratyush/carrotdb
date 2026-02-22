#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_zscore(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::ZSCORE)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        ZscoreCommand *zscoreCmd = static_cast<ZscoreCommand *>(c.command.get());
        const std::string &key = zscoreCmd->key;
        const std::string &member = zscoreCmd->member;

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
        std::optional<double> score = sortedSet->score(member);

        if (score.has_value())
        {
            std::string score_str = std::to_string(score.value());
            if (score_str.find('.') != std::string::npos)
            {
                size_t last_non_zero = score_str.find_last_not_of('0');
                if (last_non_zero != std::string::npos && score_str[last_non_zero] == '.')
                {
                    score_str = score_str.substr(0, last_non_zero);
                }
                else if (last_non_zero != std::string::npos)
                {
                    score_str = score_str.substr(0, last_non_zero + 1);
                }
            }
            encode_bulk_string(&c.client->write_buffer, score_str);
        }
        else
        {
            encode_null_bulk_string(&c.client->write_buffer);
        }

        c.current_write_position = 0;
    }
}
