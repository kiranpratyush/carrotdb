#pragma once

#include "../models/client.h"
#include "../models/resp.h"
#include <string>

using namespace SERVER_NAMESPACE;

namespace REDIS_NAMESPACE
{
    class Parser
    {
    public:
        static ParsedToken Parse(ClientContext &clientContext);
        static bool calculate_length(ClientContext &c, unsigned long &length);
    };
}
