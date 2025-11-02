#pragma once

#include <string>
#include <memory>

enum class RedisObjectEncodingType
{
    STRING,
    LIST_PACK,
};

struct RedisObject
{
    RedisObjectEncodingType type{};
    std::unique_ptr<std::string> stringPtr{};
    std::unique_ptr<unsigned char[]> listPack{};
};