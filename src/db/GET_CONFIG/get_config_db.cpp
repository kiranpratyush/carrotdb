#include "db/db.h"
#include "utils/utils.h"

namespace REDIS_NAMESPACE
{
    void DB::handleGetConfig(ClientContext &c, const ServerConfig *config)
    {
        if (c.command->type == CommandType::GETCONFIG)
        {
            GetConfigCommand *command = static_cast<GetConfigCommand *>(c.command.get());
            for (auto p : command->parameters)
            {
                if (is_equal(p, "dir"))
                {
                    encode_array_header(&c.client->write_buffer, 2);
                    encode_bulk_string(&c.client->write_buffer, "dir");
                    encode_bulk_string(&c.client->write_buffer, config->rdb_file_path);
                    c.current_write_position = 0;
                }
            }
        }
    }
}