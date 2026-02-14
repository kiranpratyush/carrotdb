#include "db/db.h"
#include <set>

namespace REDIS_NAMESPACE
{
    static std::set<CommandType>allowedPubSubCommands{
        CommandType::SUBSCRIBE,
        CommandType::PING
    };

    static std::string formatError(CommandType type)
    {
        return "Can't execute '"+ commandTypeToString(type)+"': only (P|S)SUBSCRIBE / (P|S)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context";
    }


    void DB::handleSubscribe(ClientContext &c)
    {
        SubscribeCommand *command = static_cast<SubscribeCommand*>(c.command.get());
        pubsub_clients[command->channel_name].push_back(c.client);
        c.client->setClientPubSub();
        c.client->total_subscribed_channels+=1;
        auto write_buffer = &c.client->write_buffer;
        encode_array_header(write_buffer,3);
        encode_bulk_string(write_buffer,"subscribe");
        encode_bulk_string(write_buffer,command->channel_name);
        encode_integer(write_buffer,c.client->total_subscribed_channels);
        c.current_write_position=0;
    }
    void DB::handlePubSub(ClientContext &c)
    {
        auto it = allowedPubSubCommands.find(c.command->type);
        if(it == allowedPubSubCommands.end())
        {
            /*Send Error*/
            encode_error(&c.client->write_buffer,formatError(c.command->type));
            return;
        }
        if(c.command->type == CommandType::SUBSCRIBE)
        {
            handleSubscribe(c);
        }   
        else{
            encode_array_header(&c.client->write_buffer,2);
            call(c,*c.command.get());
            encode_bulk_string(&c.client->write_buffer,"");
        }
    }

}