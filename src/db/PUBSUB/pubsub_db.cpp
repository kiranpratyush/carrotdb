#include "db/db.h"
#include <set>

namespace REDIS_NAMESPACE
{
    static std::set<CommandType>allowedPubSubCommands{
        CommandType::SUBSCRIBE,
        CommandType::PING,
        CommandType::UNSUBSCRIBE
    };

    static std::string formatError(CommandType type)
    {
        return "Can't execute '"+ commandTypeToString(type)+"': only (P|S)SUBSCRIBE / (P|S)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context";
    }

    void DB::handlePublish(ClientContext &c)
    {
        PublishCommand *command = static_cast<PublishCommand*>(c.command.get());
        auto subscribed_clients = pubsub_clients[command->channel_name];
        int active_subscribed_clients = 0;
        for(auto client:subscribed_clients)
        {
            auto clientPtr = client.lock();
            if(clientPtr)
            {   active_subscribed_clients++;
                auto *write_buffer = &clientPtr->write_buffer;
                encode_array_header(write_buffer,3);
                encode_bulk_string(write_buffer,"message");
                encode_bulk_string(write_buffer,command->channel_name);
                encode_bulk_string(write_buffer,command->message);
            }
            c.ready_to_write_clients.push_back(client);
        }
        encode_integer(&c.client->write_buffer,active_subscribed_clients);
    }

    /*TODO This is inefficient o(n) TODO optimize it*/
    void DB::handleUnSubscribe(ClientContext &c)
    {
        UnSubscribeCommand *command = static_cast<UnSubscribeCommand*>(c.command.get());
        auto *subscribedClients = &pubsub_clients[command->channel_name];
        for(auto it = subscribedClients->begin();it!=subscribedClients->end();it++)
        {
            auto client = (*it).lock();
            if(client->fd == c.client->fd)
            {
                subscribedClients->erase(it);
                c.client->total_subscribed_channels--;
                break;
            }

        }
        encode_array_header(&c.client->write_buffer,3);
        encode_bulk_string(&c.client->write_buffer,"unsubscribe");
        encode_bulk_string(&c.client->write_buffer,command->channel_name);
        encode_integer(&c.client->write_buffer,c.client->total_subscribed_channels);
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
        else if(c.command->type == CommandType::UNSUBSCRIBE)
        {
            handleUnSubscribe(c);
        }  
        else{
            encode_array_header(&c.client->write_buffer,2);
            call(c,*c.command.get());
            encode_bulk_string(&c.client->write_buffer,"");
        }
    }

}