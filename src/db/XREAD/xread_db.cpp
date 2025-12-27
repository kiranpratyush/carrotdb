#include "db/db.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_xread(ClientContext &c, int total_commands)
    {
        // Current supported form (multiple streams):
        // XREAD STREAMS key1 key2 ... id1 id2 ...
        // (Optional COUNT/BLOCK not handled here.)
        Parser::Parse(c); // ignore STREAMS
        std::string_view readBuffer = c.client->read_buffer;

        const int remaining = total_commands - 1;
        if (remaining <= 0 || (remaining % 2) != 0)
        {
            c.client->write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }

        const int numStreams = remaining / 2;
        std::vector<std::string> keys{};
        std::vector<std::string> startIds{};
        keys.reserve(numStreams);
        startIds.reserve(numStreams);

        for (int i = 0; i < numStreams; i++)
        {
            ParsedToken keyToken = Parser::Parse(c);
            keys.emplace_back(readBuffer.data() + keyToken.start_pos, keyToken.end_pos - keyToken.start_pos + 1);
        }
        for (int i = 0; i < numStreams; i++)
        {
            ParsedToken startStreamToken = Parser::Parse(c);
            startIds.emplace_back(readBuffer.data() + startStreamToken.start_pos, startStreamToken.end_pos - startStreamToken.start_pos + 1);
        }

        struct StreamReply
        {
            std::string key;
            std::vector<std::pair<std::string, std::string>> entries;
        };
        std::vector<StreamReply> replies{};
        replies.reserve(numStreams);

        for (int i = 0; i < numStreams; i++)
        {
            auto it = store.find(keys[i]);
            if (it == store.end())
                continue;
            RedisObject &redisObj = it->second;
            if (redisObj.type != RedisObjectEncodingType::STREAM || !redisObj.streamPtr)
                continue;

            std::vector<std::pair<std::string, std::string>> entries{}; // (binary_id, raw field/value bulk-string sequence)
            const bool ok = redisObj.streamPtr->xrangeData(startIds[i], "+", entries, true);
            if (!ok || entries.empty())
                continue;

            replies.push_back(StreamReply{keys[i], std::move(entries)});
        }

        if (replies.empty())
        {
            c.client->write_buffer.append("*0\r\n");
            c.current_write_position = 0;
            return;
        }

        c.client->write_buffer.append("*" + std::to_string(replies.size()) + "\r\n");
        for (const auto &reply : replies)
        {
            c.client->write_buffer.append("*2\r\n");
            c.client->write_buffer.append("$" + std::to_string(reply.key.size()) + "\r\n");
            c.client->write_buffer.append(reply.key + "\r\n");

            c.client->write_buffer.append("*" + std::to_string(reply.entries.size()) + "\r\n");
            for (const auto &[binaryId, rawPayload] : reply.entries)
            {
                StreamID sid = StreamID::fromBinary(binaryId);
                std::string sidStr = sid.toString();

                std::vector<std::string> fields{};
                (void)Parser::parse_bulk_string_sequence(rawPayload, fields);

                c.client->write_buffer.append("*2\r\n");
                c.client->write_buffer.append("$" + std::to_string(sidStr.size()) + "\r\n");
                c.client->write_buffer.append(sidStr + "\r\n");

                c.client->write_buffer.append("*" + std::to_string(fields.size()) + "\r\n");
                for (const auto &item : fields)
                {
                    c.client->write_buffer.append("$" + std::to_string(item.size()) + "\r\n");
                    c.client->write_buffer.append(item + "\r\n");
                }
            }
        }

        c.current_write_position = 0;
    }
}