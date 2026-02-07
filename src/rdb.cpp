#include "rdb.h"

namespace RDB_NAMESPACE
{
    void Rdb::parse()
    {
        io.open();

        if (!parseHeader())
        {
            std::cerr << "Failed to parse RDB header" << std::endl;
            return;
        }

        parseMetadata();

        while (parseDatabase())
        {
        }

        std::cout << "RDB parsing completed successfully" << std::endl;
    }

    void Rdb::takesnapshot()
    {
    }
    uint64_t Rdb::getLength(unsigned char byte, bool &isEncType)
    {
        unsigned char lengthType = (byte >> 6) & 0x03;

        if (lengthType == LengthType::RDB_6BITLEN)
        {
            return byte & 0x3f;
        }
        else if (lengthType == LengthType::RDB_14BITLEN)
        {
            auto nextByte = io.read(1);
            if (nextByte.size() == 0)
                return LengthType::RDB_LENERR;
            return ((byte & 0x3f) << 8) | nextByte[0];
        }
        else if (lengthType == LengthType::RDB_32BITLEN)
        {
            auto nextBytes = io.read(4);
            if (nextBytes.size() < 4)
                return LengthType::RDB_LENERR;
            return (static_cast<uint32_t>(nextBytes[0]) << 24) |
                   (static_cast<uint32_t>(nextBytes[1]) << 16) |
                   (static_cast<uint32_t>(nextBytes[2]) << 8) |
                   static_cast<uint32_t>(nextBytes[3]);
        }
        else if (lengthType == LengthType::RDB_ENCVAL)
        {
            isEncType = true;
            return LengthType::RDB_LENERR;
        }
        return LengthType::RDB_LENERR;
    }

    std::string Rdb::getSpecialString(unsigned char byte)
    {
        if (byte == EncType::RDB_ENC_INT8)
        {
            // Read 8-bit integer
            auto data = io.read(1);
            if (data.size() < 1)
                return "";
            int8_t value = static_cast<int8_t>(data[0]);
            return std::to_string(value);
        }
        else if (byte == EncType::RDB_ENC_INT16)
        {
            // Read 16-bit integer (little-endian)
            auto data = io.read(2);
            if (data.size() < 2)
                return "";
            int16_t value = static_cast<int16_t>(data[0]) |
                            (static_cast<int16_t>(data[1]) << 8);
            return std::to_string(value);
        }
        else if (byte == EncType::RDB_ENC_INT32)
        {
            // Read 32-bit integer (little-endian)
            auto data = io.read(4);
            if (data.size() < 4)
                return "";
            int32_t value = static_cast<int32_t>(data[0]) |
                            (static_cast<int32_t>(data[1]) << 8) |
                            (static_cast<int32_t>(data[2]) << 16) |
                            (static_cast<int32_t>(data[3]) << 24);
            return std::to_string(value);
        }
        else if (byte == EncType::RDB_ENC_LZF)
        {
            return "";
        }
        return "";
    }

    std::string Rdb::parseNormalString(uint64_t length)
    {
        if (length == 0)
            return "";
        auto data = io.read(static_cast<size_t>(length));

        if (data.size() != length)
            return "";

        std::string result;
        result.reserve(length);

        for (unsigned char byte : data)
        {
            result.push_back(static_cast<char>(byte));
        }

        return result;
    }

    bool Rdb::parseHeader()
    {
        // Read "REDIS" magic string (5 bytes)
        auto magicBytes = io.read(5);
        if (magicBytes.size() != 5)
            return false;

        std::string magic(magicBytes.begin(), magicBytes.end());
        if (magic != "REDIS")
        {
            std::cerr << "Invalid RDB file: magic string mismatch" << std::endl;
            return false;
        }

        // Read version (4 bytes)
        auto versionBytes = io.read(4);
        if (versionBytes.size() != 4)
            return false;

        std::string version(versionBytes.begin(), versionBytes.end());
        std::cout << "RDB Version: " << version << std::endl;

        return true;
    }

    void Rdb::parseMetadata()
    {
        while (true)
        {
            auto opcodeBytes = io.read(1);
            if (opcodeBytes.empty())
                break;

            unsigned char opcode = opcodeBytes[0];

            if (opcode == RDBConstants::RDB_OPCODE_AUX)
            {
                std::string metaKey = parseString();
                std::string metaValue = parseString();
                std::cout << "Metadata: " << metaKey << " = " << metaValue << std::endl;
            }
            else
            {
                io.seekBackRead(1);
                break;
            }
        }
    }

    bool Rdb::parseDatabase()
    {
        auto opcodeBytes = io.read(1);
        if (opcodeBytes.empty())
            return false;

        unsigned char opcode = opcodeBytes[0];

        if (opcode == RDBConstants::RDB_OPCODE_EOF)
        {
            std::cout << "Reached end of RDB file" << std::endl;
            return false;
        }

        if (opcode == RDBConstants::RDB_OPCODE_SELECTDB)
        {
            bool isEncType = false;
            uint64_t dbNum = getLength(io.read(1)[0], isEncType);
            std::cout << "Selecting database: " << dbNum << std::endl;

            auto nextOpcode = io.read(1);
            if (!nextOpcode.empty() && nextOpcode[0] == RDBConstants::RDB_OPCODE_RESIZEDB)
            {
                bool dummy;
                uint64_t hashTableSize = getLength(io.read(1)[0], dummy);
                uint64_t expireTableSize = getLength(io.read(1)[0], dummy);
                std::cout << "Hash table size: " << hashTableSize << ", Expire table size: " << expireTableSize << std::endl;
            }
        }
        while (true)
        {
            auto keyOpcodeBytes = io.read(1);
            if (keyOpcodeBytes.empty())
                return false;

            unsigned char keyOpcode = keyOpcodeBytes[0];

            if (keyOpcode == RDBConstants::RDB_OPCODE_SELECTDB || keyOpcode == RDBConstants::RDB_OPCODE_EOF)
            {
                io.seekBackRead(1);
                return true;
            }

            uint64_t expiry = 0;
            bool hasExpiry = false;

            if (keyOpcode == RDBConstants::RDB_OPCODE_EXPIRETIME_MS || keyOpcode == RDBConstants::RDB_OPCODE_EXPIRETIME)
            {
                expiry = parseExpiry(keyOpcode);
                hasExpiry = true;
                auto typeBytes = io.read(1);
                if (typeBytes.empty())
                    return false;
                keyOpcode = typeBytes[0];
            }

            if (keyOpcode == RDBConstants::RDB_TYPE_STRING)
            {
                std::string key = parseString();
                std::string value = parseString();

                if (auto dbPtr = db.lock())
                {
                    if (hasExpiry)
                    {
                        dbPtr->setWithExpiry(key, value, expiry);
                        std::cout << "Stored key: " << key << " = " << value << " (expires: " << expiry << ")" << std::endl;
                    }
                    else
                    {
                        dbPtr->set(key, value);
                        std::cout << "Stored key: " << key << " = " << value << std::endl;
                    }
                }
            }
            else
            {
                std::cerr << "Unsupported RDB type: " << static_cast<int>(keyOpcode) << std::endl;
                return false;
            }
        }

        return true;
    }

    std::string Rdb::parseString()
    {
        auto lengthBytes = io.read(1);
        if (lengthBytes.empty())
            return "";

        bool isEncType = false;
        uint64_t length = getLength(lengthBytes[0], isEncType);

        if (isEncType)
        {
            return getSpecialString(lengthBytes[0]);
        }
        else
        {
            return parseNormalString(length);
        }
    }

    uint64_t Rdb::parseExpiry(unsigned char opcode)
    {
        if (opcode == RDBConstants::RDB_OPCODE_EXPIRETIME_MS)
        {
            auto expiryBytes = io.read(8);
            if (expiryBytes.size() < 8)
                return 0;

            uint64_t expiry = 0;
            for (int i = 0; i < 8; i++)
            {
                expiry |= (static_cast<uint64_t>(expiryBytes[i]) << (i * 8));
            }
            return expiry;
        }
        else if (opcode == RDBConstants::RDB_OPCODE_EXPIRETIME)
        {
            auto expiryBytes = io.read(4);
            if (expiryBytes.size() < 4)
                return 0;

            uint32_t expiry = 0;
            for (int i = 0; i < 4; i++)
            {
                expiry |= (static_cast<uint32_t>(expiryBytes[i]) << (i * 8));
            }
            return static_cast<uint64_t>(expiry) * 1000;
        }

        return 0;
    }

}