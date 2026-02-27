#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"
#include "geohash.h"

namespace REDIS_NAMESPACE
{
    void DB::handle_geosearch(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::GEOSEARCH)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        GeoSearchCommand *geoSearchCmd = static_cast<GeoSearchCommand *>(c.command.get());
        const std::string &key = geoSearchCmd->key;
        double centerLon = geoSearchCmd->longitude;
        double centerLat = geoSearchCmd->latitude;
        double radius = geoSearchCmd->radius;

        auto it = store.find(key);

        if (it == store.end())
        {
            encode_empty_array(&c.client->write_buffer);
            c.current_write_position = 0;
            return;
        }

        if (it->second.type != RedisObjectEncodingType::SORTED_SET)
        {
            encode_error(&c.client->write_buffer, "WRONGTYPE Operation against a key holding the wrong kind of value");
            c.current_write_position = 0;
            return;
        }

        SortedSet *sortedSet = it->second.sortedSetPtr.get();

        auto allMembers = sortedSet->getAllWithScores();

        GEO_HASH_NAMESPACE::GeoHash geoHash(
            {GeoAddCommand::minAllowedLat, GeoAddCommand::maxAllowedLat},
            {GeoAddCommand::minAllowedLon, GeoAddCommand::maxAllowedLon});

        GEO_HASH_NAMESPACE::GeoHashArea centerArea;
        GEO_HASH_NAMESPACE::GeoHashBits centerHashBits;
        centerHashBits.bits = 0;
        centerHashBits.step = 26;
        geoHash.geoHashEncode(centerLon, centerLat, 26, &centerHashBits);
        geoHash.geoHashDecode(centerHashBits, &centerArea);

        std::vector<std::string> results;

        for (const auto &[member, score] : allMembers)
        {
            GEO_HASH_NAMESPACE::GeoHashBits hashBits;
            hashBits.bits = static_cast<uint64_t>(score);
            hashBits.step = 26;

            GEO_HASH_NAMESPACE::GeoHashArea hashArea;
            bool success = geoHash.geoHashDecode(hashBits, &hashArea);

            if (!success)
            {
                continue;
            }

            double distance = geoHash.calculateDistance(centerArea, hashArea);

            if (distance <= radius)
            {
                results.push_back(member);
            }
        }

        encode_bulk_string_array(&c.client->write_buffer, results);
        c.current_write_position = 0;
    }
}
