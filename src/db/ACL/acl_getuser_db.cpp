#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_acl_getuser(ClientContext &c)
    {
        ACLGetUserCommand *cmd = static_cast<ACLGetUserCommand *>(c.command.get());
        std::string username = cmd->username.empty() ? "default" : cmd->username;
        
        std::vector<std::string> user_flags;
        if (c.client->user)
        {
            user_flags = c.client->user->flags;
        }

        encode_array_header(&c.client->write_buffer, 2);
        encode_bulk_string(&c.client->write_buffer, "flags");
        encode_bulk_string_array(&c.client->write_buffer, user_flags);
        c.current_write_position = 0;
    }
}
