#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"
#include "acl.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_acl_setuser(ClientContext &c,const ServerConfig *config)
    {
        ACLSetUserCommand *cmd = static_cast<ACLSetUserCommand *>(c.command.get());
        
        std::string hashedPassword;
        if (cmd->password.has_value())
        {
            hashedPassword = hash_sha256(cmd->password.value());
        }
        
        config->aclManager->setUser(cmd->username,hashedPassword,{});
    
        encode_simple_string(&c.client->write_buffer, "OK");
        c.current_write_position = 0;
    }
}
