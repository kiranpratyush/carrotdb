#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"
#include "geohash.h"
#include <sstream>
#include <iomanip>

namespace REDIS_NAMESPACE
{
    void DB::handle_geodist(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::GEODIST)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        GeoDistCommand *geoDistCmd = static_cast<GeoDistCommand *>(c.command.get());
        const std::string &key = geoDistCmd->key;
        const std::string &member1 = geoDistCmd->member1;
        const std::string &member2 = geoDistCmd->member2;

        auto it = store.find(key);

        if (it == store.end())
        {
            encode_null_bulk_string(&c.client->write_buffer);
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

        std::optional<double> score1 = sortedSet->score(member1);
        std::optional<double> score2 = sortedSet->score(member2);

        if (!score1.has_value() || !score2.has_value())
        {
            encode_null_bulk_string(&c.client->write_buffer);
            c.current_write_position = 0;
            return;
        }

        GEO_HASH_NAMESPACE::GeoHash geoHash(
            {GeoAddCommand::minAllowedLat, GeoAddCommand::maxAllowedLat},
            {GeoAddCommand::minAllowedLon, GeoAddCommand::maxAllowedLon});

        GEO_HASH_NAMESPACE::GeoHashBits hashBits1;
        hashBits1.bits = static_cast<uint64_t>(score1.value());
        hashBits1.step = 26;

        GEO_HASH_NAMESPACE::GeoHashBits hashBits2;
        hashBits2.bits = static_cast<uint64_t>(score2.value());
        hashBits2.step = 26;

        GEO_HASH_NAMESPACE::GeoHashArea hashArea1;
        GEO_HASH_NAMESPACE::GeoHashArea hashArea2;

        bool success1 = geoHash.geoHashDecode(hashBits1, &hashArea1);
        bool success2 = geoHash.geoHashDecode(hashBits2, &hashArea2);

        if (!success1 || !success2)
        {
            encode_null_bulk_string(&c.client->write_buffer);
            c.current_write_position = 0;
            return;
        }

        double distance = geoHash.calculateDistance(hashArea1, hashArea2);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << distance;
        std::string distance_str = oss.str();

        encode_bulk_string(&c.client->write_buffer, distance_str);
        c.current_write_position = 0;
    }
}
