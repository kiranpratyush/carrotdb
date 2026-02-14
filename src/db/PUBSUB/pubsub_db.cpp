#include "db/db.h"

namespace REDIS_NAMESPACE
{
    void DB::handleSubscribe(ClientContext &c)
    {
        SubscribeCommand *command = static_cast<SubscribeCommand*>(c.command.get());
        pubsub_clients[command->channel_name].push_back(c.client);
        c.client->setClientPubSub();
        c.client->total_subscribed_channels+=1;
        auto write_buffer = &c.client->write_buffer;
        encode_array_header(write_buffer,3);
        encode_bulk_string(write_buffer,"SUBSCRIBE");
        encode_bulk_string(write_buffer,command->channel_name);
        encode_integer(write_buffer,c.client->total_subscribed_channels);
        c.current_write_position=0;
    }

}