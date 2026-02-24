#include "geohash.h"

namespace GEO_HASH_NAMESPACE
{
    /*Idea: Always recursively swap second and third quartile of bits
      Total bytes of interleaved data is 64bits ,but we are taking 32 bit input
      so the other 32 bits will be zero
      like 0000..(32bits),bbbbbb..(32bits)
      1st pass: 0000000000000000 0000000000000000 bbbbbbbbbbbbbbbb bbbbbbbbbbbbbbbb
      swap    : 0000000000000000 bbbbbbbbbbbbbbbb 000000000000000  bbbbbbbbbbbbbbbb
      for this move 16 bit left shift and carve out the only bits part and take or to take everything
      and then do bit wise and  with 0000FFFF0000FFFFULL to keep the swapped bits
      Recursively continue with 2nd and 3rd quartile
    */
    uint64_t GeoHash::interleave64(uint32_t xlo, uint32_t ylo)
    {
        static const uint64_t B[] = {0x5555555555555555ULL, 0x3333333333333333ULL,
                                     0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL,
                                     0x0000FFFF0000FFFFULL};
        static const unsigned int S[] = {1, 2, 4, 8, 16};

        uint64_t x = xlo;
        uint64_t y = ylo;

        x = (x | (x << S[4])) & B[4];
        y = (y | (y << S[4])) & B[4];

        x = (x | (x << S[3])) & B[3];
        y = (y | (y << S[3])) & B[3];

        x = (x | (x << S[2])) & B[2];
        y = (y | (y << S[2])) & B[2];

        x = (x | (x << S[1])) & B[1];
        y = (y | (y << S[1])) & B[1];

        x = (x | (x << S[0])) & B[0];
        y = (y | (y << S[0])) & B[0];

        return x | (y << 1);
    }
    uint64_t GeoHash::deinterleave64(uint64_t interleaved)
    {
        static const uint64_t B[] = {0x5555555555555555ULL, 0x3333333333333333ULL,
                                     0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL,
                                     0x0000FFFF0000FFFFULL, 0x00000000FFFFFFFFULL};
        static const unsigned int S[] = {0, 1, 2, 4, 8, 16};

        uint64_t x = interleaved;
        uint64_t y = interleaved >> 1;

        x = (x | (x >> S[0])) & B[0];
        y = (y | (y >> S[0])) & B[0];

        x = (x | (x >> S[1])) & B[1];
        y = (y | (y >> S[1])) & B[1];

        x = (x | (x >> S[2])) & B[2];
        y = (y | (y >> S[2])) & B[2];

        x = (x | (x >> S[3])) & B[3];
        y = (y | (y >> S[3])) & B[3];

        x = (x | (x >> S[4])) & B[4];
        y = (y | (y >> S[4])) & B[4];

        x = (x | (x >> S[5])) & B[5];
        y = (y | (y >> S[5])) & B[5];

        return x | (y << 32);
    }

    bool GeoHash::geoHashEncode(double longitude, double latitude, uint8_t step, GeoHashBits *hash)
    {
        if (longitude > longRange.max || longitude < longRange.min || latitude > latRange.max || latitude < latRange.min)
        {
            return false;
        }
        hash->bits = 0;
        hash->step = step;
        double lat_offset = (latitude - latRange.min) / (latRange.max - latRange.min);
        double long_offset = (longitude - longRange.min) / (longRange.max - longRange.min);
        /*Usually the step size is 26 to be able to store the double without losing precesion,mantissa 53 in IEEE format*/
        lat_offset *= (1 << step);
        long_offset *= (1 << step);
        hash->bits = interleave64(lat_offset, long_offset);
        return 1;
    }
}