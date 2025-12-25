#pragma once

#include <string>
#include <memory>
#include "radix-tree.h"

enum class RedisObjectEncodingType
{
    STRING,
    LIST_PACK,
    STREAM
};

struct RedisObject
{
    RedisObjectEncodingType type{};
    std::unique_ptr<std::string> stringPtr{};
    std::unique_ptr<unsigned char[]> listPack{};
    std::unique_ptr<RadixTree> radixTreePtr{};
};