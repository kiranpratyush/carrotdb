#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"
#include "acl.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_auth(ClientContext &c, const ServerConfig *config)
    {
        AuthCommand *cmd = static_cast<AuthCommand *>(c.command.get());

        auto user = config->aclManager->getUser(cmd->username);

        if (!user || !user->is_enabled)
        {
            encode_error(&c.client->write_buffer, "WRONGPASS invalid username-password pair or user is disabled");
            c.current_write_position = 0;
            return;
        }

        if (user->nopass)
        {
            c.client->user = user;
            encode_simple_string(&c.client->write_buffer, "OK");
            c.current_write_position = 0;
            return;
        }

        std::string hashedPassword = hash_sha256(cmd->password);
        bool passwordMatch = false;
        for (const auto &storedPassword : user->passwords)
        {
            if (storedPassword == hashedPassword)
            {
                passwordMatch = true;
                break;
            }
        }

        if (!passwordMatch)
        {
            encode_error(&c.client->write_buffer, "WRONGPASS invalid username-password pair or user is disabled");
            c.current_write_position = 0;
            return;
        }

        c.client->user = user;
        encode_simple_string(&c.client->write_buffer, "OK");
        c.current_write_position = 0;
    }
}
