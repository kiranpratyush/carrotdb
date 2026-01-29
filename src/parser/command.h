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
        UNKNOWN,
        MULTI,
        EXEC,
        DISCARD,
        INFO,
        REPLCONF,
        PSYNC
    };

    // Base command structure
    struct Command
    {
        CommandType type{};
        std::string key{};
        virtual ~Command() = default;
        virtual bool is_write_command() const = 0;
        virtual std::string to_resp() const = 0;
    };
    struct UnknowCommand : public Command
    {
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }

        UnknowCommand() { type = CommandType::UNKNOWN; }
    };

    // PING command (no parameters)
    struct PingCommand : public Command
    {
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
        PingCommand() { type = CommandType::PING; }
    };

    // ECHO command
    struct EchoCommand : public Command
    {
        EchoCommand() { type = CommandType::ECHO; }
        std::string message{};
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // SET command with EX and PX flags
    struct SetCommand : public Command
    {
        SetCommand() { type = CommandType::SET; }
        std::string value{};
        std::optional<uint64_t> ex{}; // Expiration in seconds
        std::optional<uint64_t> px{}; // Expiration in milliseconds
        bool is_write_command() const override
        {
            return true;
        }
        std::string to_resp() const override
        {
            int initial_command_count = 3;
            if (ex.has_value())
                initial_command_count += 2;
            if (px.has_value())
                initial_command_count += 2;
            std::string cmd = "*" + std::to_string(initial_command_count) + "\r\n";
            cmd += "$3\r\nSET\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            cmd += "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
            if (ex.has_value())
            {
                cmd += "$2\r\nEX\r\n";
                cmd += "$" + std::to_string(std::to_string(*ex).length()) +
                       "\r\n" + std::to_string(*ex) + "\r\n";
            }
            if (px.has_value())
            {
                cmd += "$2\r\nPX\r\n";
                cmd += "$" + std::to_string(std::to_string(*px).length()) +
                       "\r\n" + std::to_string(*px) + "\r\n";
            }
            return cmd;
        }
    };

    // GET command
    struct GetCommand : public Command
    {
        GetCommand() { type = CommandType::GET; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // TYPE command
    struct TypeCommand : public Command
    {
        TypeCommand() { type = CommandType::TYPE; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // INCR command
    struct IncrCommand : public Command
    {
        IncrCommand() { type = CommandType::INCR; }
        bool is_write_command() const override
        {
            return true;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // RPUSH command
    struct RpushCommand : public Command
    {
        RpushCommand() { type = CommandType::RPUSH; }
        std::vector<std::string> values{};
        bool is_write_command() const override
        {
            return true;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // LPUSH command
    struct LpushCommand : public Command
    {
        LpushCommand() { type = CommandType::LPUSH; }
        std::vector<std::string> values{};
        bool is_write_command() const override
        {
            return true;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // LRANGE command
    struct LrangeCommand : public Command
    {
        LrangeCommand() { type = CommandType::LRANGE; }
        int64_t start{};
        int64_t stop{};
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // LLEN command
    struct LlenCommand : public Command
    {
        LlenCommand() { type = CommandType::LLEN; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // LPOP command with optional count parameter
    struct LpopCommand : public Command
    {
        LpopCommand() { type = CommandType::LPOP; }
        std::optional<uint32_t> count{}; // Optional count parameter
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // BLPOP command with timeout
    struct BlpopCommand : public Command
    {
        BlpopCommand() { type = CommandType::BLPOP; }
        std::vector<std::string> keys{}; // Can block on multiple keys
        double timeout{};                // Timeout in seconds (0 = infinite)
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // XADD command
    struct XaddCommand : public Command
    {
        XaddCommand() { type = CommandType::XADD; }
        std::string stream_id{}; // Can be "*" for auto-generation
        std::vector<std::pair<std::string, std::string>> field_values{};
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // XRANGE command
    struct XrangeCommand : public Command
    {
        XrangeCommand() { type = CommandType::XRANGE; }
        std::string start{}; // Can be "-" for minimum
        std::string end{};   // Can be "+" for maximum
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

    // XREAD command with BLOCK flag
    struct XreadCommand : public Command
    {
        XreadCommand() { type = CommandType::XREAD; }
        std::optional<double> block_timeout{}; // BLOCK flag timeout in milliseconds (0 = infinite)
        std::vector<std::string> keys{};
        std::vector<std::string> ids{}; // Can be "$" for latest
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };
    // MULTI command
    struct MultiCommand : public Command
    {
        MultiCommand() { type = CommandType::MULTI; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };
    // EXEC command
    struct ExecCommand : public Command
    {
        ExecCommand() { type = CommandType::EXEC; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };
    // DISCARD command
    struct DiscardCommand : public Command
    {
        DiscardCommand() { type = CommandType::DISCARD; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };
    // INFO command
    struct InfoCommand : public Command
    {
        InfoCommand() { type = CommandType::INFO; }
        bool isReplicationArgument{false};
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };
    struct ReplConfCommand : public Command
    {
        std::string listening_port;
        std::string capability;

        ReplConfCommand()
        {
            type = CommandType::REPLCONF;
        }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };
    struct PsyncCommand : public Command
    {
        std::string repl_id{};
        int64_t offset{};
        PsyncCommand()
        {
            type = CommandType::PSYNC;
        }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };

}