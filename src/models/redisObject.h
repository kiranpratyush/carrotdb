#pragma once

#include <string>
#include <memory>
#include "stream.h"
#include "sortedset.h"

enum class RedisObjectEncodingType
{
    STRING,
    LIST_PACK,
    STREAM,
    SORTED_SET
};

namespace REDIS_NAMESPACE
{
    struct RedisObject
    {
        RedisObjectEncodingType type{};
        std::unique_ptr<std::string> stringPtr{};
        std::unique_ptr<unsigned char[]> listPack{};
        std::unique_ptr<Stream> streamPtr{};
        std::unique_ptr<SortedSet> sortedSetPtr{};
    };
}
