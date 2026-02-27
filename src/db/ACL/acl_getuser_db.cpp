#include "db/db.h"
#include "utils/utils.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_acl_getuser(ClientContext &c)
    {
        std::string username = "default";
        if (c.client->user)
        {
            username = c.client->user->username;
        }

        encode_array_header(&c.client->write_buffer, 2);
        encode_bulk_string(&c.client->write_buffer, "flags");
        encode_empty_array(&c.client->write_buffer);
        c.current_write_position = 0;
    }
}
