/*This is heavily taken from redis source code for learning purpose
  I have heavily commented the implementation for easier understanding and learning purpose
  Good luck:)
*/
#ifndef GEO_HASH
#define GEO_HASH
#include <cstdint>
namespace GEO_HASH_NAMESPACE
{
    struct GeoHashRange
    {
        double min;
        double max;
    };
    struct GeoHashBits
    {
        uint64_t bits; /*Actual interleaved bits of longitude and latitude*/
        uint8_t step;  /*Scaling factor*/
    };
    struct GeoHashArea{
        GeoHashBits hashbits;
        GeoHashRange longitude;
        GeoHashRange latitude;
    };

    class GeoHash
    {
    private:
        GeoHashRange latRange{-90.0, 90.0};
        GeoHashRange longRange{-180.0, 180.0};
        uint64_t interleave64(uint32_t xlo, uint32_t ylo);
        uint64_t deinterleave64(uint64_t interleaved);

    public:
        GeoHash() = default;
        GeoHash(GeoHashRange latRange, GeoHashRange longRange)
            : latRange(latRange), longRange(longRange) {}

        bool geoHashEncode(double longitude, double latitude, uint8_t step, GeoHashBits *hash);
        bool geoHashDecode(GeoHashBits hash,GeoHashArea *area);
    };
}

#endif