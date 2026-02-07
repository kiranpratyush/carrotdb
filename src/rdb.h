#ifndef RDB_H
#define RDB_H

#include <memory>
#include "io.h"
#include "db/db.h"

using namespace IO_NAMESPACE;
namespace RDB_NAMESPACE
{
    /* REDIS checks most significant 2 bits of the first bytes to interpret
     * the length. (Taken from original Redis implementation for learning purpose)
     * 00|XXXXXX => if the two MSB are 00 the len is the 6 bits of this byte
     * 01|XXXXXX XXXXXXXX =>  01, the len is 14 bits, 6 bits + 8 bits of next byte
     * 10|000000 [32 bit integer] => A full 32 bit len in net byte order will follow
     * 10|000001 [64 bit integer] => A full 64 bit len in net byte order will follow
     * 11|OBKIND this means: specially encoded object will follow. The six bits
     *           number specify the kind of object that follows.
     *           See the RDB_ENC_* defines.
     *
     * Lengths up to 63 are stored using a single byte, most DB keys, and may
     * values, will fit inside.*/
    struct EncType
    {
        static const unsigned char RDB_ENC_INT8 = 0xC0;  // 8-bit integer
        static const unsigned char RDB_ENC_INT16 = 0xC1; // 16-bit integer
        static const unsigned char RDB_ENC_INT32 = 0xC2; // 32-bit integer
        static const unsigned char RDB_ENC_LZF = 0xC3;   // LZF compressed string (not handled)
    };

    struct RDBConstants
    {
        static const unsigned char RDB_OPCODE_AUX = 0xFA;           // Auxiliary metadata
        static const unsigned char RDB_OPCODE_EXPIRETIME_MS = 0xFC; // Expire time in milliseconds
        static const unsigned char RDB_OPCODE_EXPIRETIME = 0xFD;    // Expire time in seconds
        static const unsigned char RDB_OPCODE_SELECTDB = 0xFE;      // Database selector
        static const unsigned char RDB_OPCODE_EOF = 0xFF;           // End of file
        static const unsigned char RDB_OPCODE_RESIZEDB = 0xFB;      // Hash table resize info
        static const unsigned char RDB_TYPE_STRING = 0x00;          // String type
    };

    struct LengthType
    {
        static const unsigned char RDB_6BITLEN = 0;
        static const unsigned char RDB_14BITLEN = 1;
        static const unsigned char RDB_32BITLEN = 2;
        static const unsigned char RDB_64BITLEN = 0x81;
        static const unsigned char RDB_ENCVAL = 3;
        static const uint64_t RDB_LENERR = UINT64_MAX;
    };
    class Rdb
    {
    private:
        RIO io;
        std::weak_ptr<DB> db;
        uint64_t getLength(unsigned char byte, bool &isEncType);
        std::string getSpecialString(unsigned char byte);
        std::string parseNormalString(uint64_t length);
        bool parseHeader();
        void parseMetadata();
        bool parseDatabase();
        std::string parseString();
        uint64_t parseExpiry(unsigned char opcode);

    public:
        Rdb() : io{}
        {
        }
        inline void setFilePath(std::string filePath)
        {
            io.setFilePath(filePath);
        }
        inline void setDBInstance(std::shared_ptr<DB> db)
        {
            this->db = db;
        }
        void parse();
        void takesnapshot();
    };
}

#endif