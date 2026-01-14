#include "replication.h"
#include "utils/utils.h"
namespace REPLICATION_NAMESPACE
{
    void replication_handshake(Client *client)
    {
        encode_array_header(&client->write_buffer, 1);
        encode_bulk_string(&client->write_buffer, "PING");
    }
}
