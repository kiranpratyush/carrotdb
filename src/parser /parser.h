#pragma once

#include <string>
namespace REDIS_NAMESPACE
{

    class Parser
    {
        static std::string_view encode(std::string_view input);
        static std::string decode();
    };

}
