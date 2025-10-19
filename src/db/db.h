#pragma once
#include "../parser/parser.h"
#include "../utils/utils.h"
#include <string>
#include <unordered_map>
#include <cctype>
#include <chrono>

namespace REDIS_NAMESPACE
{

    class DB
    {
    public:
        void execute(ClientContext &context);

    private:
        std::unordered_map<std::string, std::string> store{};
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiration{};
        bool is_equal(std::string_view s1, std::string_view s2);
        void handle_ping(ClientContext &context);
        void handle_echo(ClientContext &context);
        void handle_set(ClientContext &context, int total_commands);
        void handle_get(ClientContext &context);
    };
}
