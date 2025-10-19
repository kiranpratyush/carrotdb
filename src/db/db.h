#pragma once
#include "../parser/parser.h"
#include <string>
#include <unordered_map>
#include <cctype>

namespace REDIS_NAMESPACE
{

    class DB
    {
    public:
        void execute(ClientContext &context);

    private:
        std::unordered_map<std::string, std::string> store{};
        bool is_equal(std::string_view s1, std::string_view s2);
        void handle_ping(ClientContext &context);
        void handle_echo(ClientContext &context);
        void handle_set(ClientContext &context);
        void handle_get(ClientContext &context);
    };
}
