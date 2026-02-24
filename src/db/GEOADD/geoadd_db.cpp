#include <cstdio>
#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_geoadd(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::GEOADD)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        GeoAddCommand *geoCmd = static_cast<GeoAddCommand *>(c.command.get());

        if (geoCmd->invalidLatLon)
        {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "invalid longitude,latitude pair %f,%f",
                          geoCmd->longitude, geoCmd->latitude);
            encode_error(&c.client->write_buffer, buf);
            c.current_write_position = 0;
            return;
        }

        const std::string &key = geoCmd->key;

        auto it = store.find(key);

        if (it == store.end())
        {
            RedisObject redisObject{};
            redisObject.type = RedisObjectEncodingType::SORTED_SET;
            redisObject.sortedSetPtr = std::make_unique<SortedSet>();
            store[key] = std::move(redisObject);
            it = store.find(key);
        }

        if (it->second.type != RedisObjectEncodingType::SORTED_SET)
        {
            encode_error(&c.client->write_buffer, "WRONGTYPE Operation against a key holding the wrong kind of value");
            c.current_write_position = 0;
            return;
        }

        SortedSet *sortedSet = it->second.sortedSetPtr.get();

        // TODO: replace hardcoded score with real geohash encoding
        constexpr double score = 1.0;
        bool wasKeyExisted = sortedSet->insert(geoCmd->member, score);
        int added = wasKeyExisted ? 0 : 1;

        encode_integer(&c.client->write_buffer, static_cast<long long>(added));
        c.current_write_position = 0;
    }
}