#include "parser.h"

namespace REDIS_NAMESPACE
{
    std::string_view Parser::encode(std::string_view input)
    {
        return "Hello";
    }
    std::string Parser::decode()
    {
        return "+PONG\r\n";
    }
}