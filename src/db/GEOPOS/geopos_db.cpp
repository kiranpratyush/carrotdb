#include "db/db.h"
#include "utils/utils.h"
#include "parser/command.h"
#include "geohash.h"
#include <sstream>
#include <iomanip>

namespace REDIS_NAMESPACE
{
    void DB::handle_geopos(ClientContext &c)
    {
        if (!c.command || c.command->type != CommandType::GEOPOS)
        {
            encode_error(&c.client->write_buffer, "Invalid command type");
            c.current_write_position = 0;
            return;
        }

        GeoPosCommand *geoPosCmd = static_cast<GeoPosCommand *>(c.command.get());
        const std::string &key = geoPosCmd->key;
        const std::vector<std::string> &members = geoPosCmd->members;

        auto it = store.find(key);

        if (it == store.end())
        {
            encode_array_header(&c.client->write_buffer, members.size());
            for (size_t i = 0; i < members.size(); i++)
            {
                encode_null_bulk_string(&c.client->write_buffer);
            }
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

        GEO_HASH_NAMESPACE::GeoHash geoHash(
            {GeoAddCommand::minAllowedLat, GeoAddCommand::maxAllowedLat},
            {GeoAddCommand::minAllowedLon, GeoAddCommand::maxAllowedLon});

        encode_array_header(&c.client->write_buffer, members.size());

        for (const auto &member : members)
        {
            std::optional<double> score = sortedSet->score(member);

            if (!score.has_value())
            {
                encode_null_bulk_string(&c.client->write_buffer);
                continue;
            }

            GEO_HASH_NAMESPACE::GeoHashBits hashBits;
            hashBits.bits = static_cast<uint64_t>(score.value());
            hashBits.step = 26;

            GEO_HASH_NAMESPACE::GeoHashArea hashArea;
            bool success = geoHash.geoHashDecode(hashBits, &hashArea);

            if (!success)
            {
                encode_null_bulk_string(&c.client->write_buffer);
                continue;
            }

            double longitude = (hashArea.longitude.min + hashArea.longitude.max) / 2.0;
            double latitude = (hashArea.latitude.min + hashArea.latitude.max) / 2.0;

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(14) << longitude;
            std::string lon_str = oss.str();

            oss.str("");
            oss.clear();
            oss << std::fixed << std::setprecision(14) << latitude;
            std::string lat_str = oss.str();

            encode_array_header(&c.client->write_buffer, 2);
            encode_bulk_string(&c.client->write_buffer, lon_str);
            encode_bulk_string(&c.client->write_buffer, lat_str);
        }

        c.current_write_position = 0;
    }
}
