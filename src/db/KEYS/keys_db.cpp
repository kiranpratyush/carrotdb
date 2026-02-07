#include "db/db.h"
#include "utils/utils.h"

namespace REDIS_NAMESPACE
{

    void DB::handleKeys(ClientContext &c)
    {
        if (c.command->type == CommandType::KEYS)
        {
            KeysCommand *keysCmd = static_cast<KeysCommand *>(c.command.get());

            if (keysCmd->pattern == "*")
            {
                std::vector<std::string> keys;
                for (const auto &pair : store)
                {
                    keys.push_back(pair.first);
                }
                encode_array_header(&c.client->write_buffer, keys.size());
                for (const auto &key : keys)
                {
                    encode_bulk_string(&c.client->write_buffer, key);
                }
            }
            else
            {
                encode_empty_array(&c.client->write_buffer);
            }
        }
    }
}