#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace REDIS_NAMESPACE
{
    enum struct CommandType
    {
        ECHO,
        PING,
        SET,
        GET,
        TYPE,
        RPUSH,
        LPUSH,
        LRANGE,
        LLEN,
        LPOP,
        BLPOP,
        XADD,
        XRANGE,
        XREAD,
        INCR,
        UNKNOWN
    };

    // Base command structure
    struct Command
    {
        CommandType type{};
        virtual ~Command() = default;
    };
    struct UnknowCommand : public Command
    {
        UnknowCommand() { type = CommandType::UNKNOWN; }
    };

    // PING command (no parameters)
    struct PingCommand : public Command
    {
        PingCommand() { type = CommandType::PING; }
    };

    // ECHO command
    struct EchoCommand : public Command
    {
        EchoCommand() { type = CommandType::ECHO; }
        std::string message{};
    };

    // SET command with EX and PX flags
    struct SetCommand : public Command
    {
        SetCommand() { type = CommandType::SET; }
        std::string key{};
        std::string value{};
        std::optional<uint64_t> ex{}; // Expiration in seconds
        std::optional<uint64_t> px{}; // Expiration in milliseconds
    };

    // GET command
    struct GetCommand : public Command
    {
        GetCommand() { type = CommandType::GET; }
        std::string key{};
    };

    // TYPE command
    struct TypeCommand : public Command
    {
        TypeCommand() { type = CommandType::TYPE; }
        std::string key{};
    };

    // INCR command
    struct IncrCommand : public Command
    {
        IncrCommand() { type = CommandType::INCR; }
        std::string key{};
    };

    // RPUSH command
    struct RpushCommand : public Command
    {
        RpushCommand() { type = CommandType::RPUSH; }
        std::string key{};
        std::vector<std::string> values{};
    };

    // LPUSH command
    struct LpushCommand : public Command
    {
        LpushCommand() { type = CommandType::LPUSH; }
        std::string key{};
        std::vector<std::string> values{};
    };

    // LRANGE command
    struct LrangeCommand : public Command
    {
        LrangeCommand() { type = CommandType::LRANGE; }
        std::string key{};
        int64_t start{};
        int64_t stop{};
    };

    // LLEN command
    struct LlenCommand : public Command
    {
        LlenCommand() { type = CommandType::LLEN; }
        std::string key{};
    };

    // LPOP command with optional count parameter
    struct LpopCommand : public Command
    {
        LpopCommand() { type = CommandType::LPOP; }
        std::string key{};
        std::optional<uint32_t> count{}; // Optional count parameter
    };

    // BLPOP command with timeout
    struct BlpopCommand : public Command
    {
        BlpopCommand() { type = CommandType::BLPOP; }
        std::vector<std::string> keys{}; // Can block on multiple keys
        double timeout{};                // Timeout in seconds (0 = infinite)
    };

    // XADD command
    struct XaddCommand : public Command
    {
        XaddCommand() { type = CommandType::XADD; }
        std::string key{};
        std::string stream_id{}; // Can be "*" for auto-generation
        std::vector<std::pair<std::string, std::string>> field_values{};
    };

    // XRANGE command
    struct XrangeCommand : public Command
    {
        XrangeCommand() { type = CommandType::XRANGE; }
        std::string key{};
        std::string start{}; // Can be "-" for minimum
        std::string end{};   // Can be "+" for maximum
    };

    // XREAD command with BLOCK flag
    struct XreadCommand : public Command
    {
        XreadCommand() { type = CommandType::XREAD; }
        std::optional<double> block_timeout{}; // BLOCK flag timeout in milliseconds (0 = infinite)
        std::vector<std::string> keys{};
        std::vector<std::string> ids{}; // Can be "$" for latest
    };

}