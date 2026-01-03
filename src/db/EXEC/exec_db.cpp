#include "db/db.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_exec(ClientContext &context, const ExecCommand &cmd)
    {
        auto clientPtr = context.client.get();

        if (clientPtr && !clientPtr->isClientOnTransaction())
        {
            encode_error(&clientPtr->write_buffer, "EXEC without MULTI");
            return;
        }
    }

}