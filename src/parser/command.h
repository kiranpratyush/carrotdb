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
        PSYNC,
        WAIT,
        GETCONFIG,
        KEYS,
        SUBSCRIBE,
        PUBLISH,
        UNSUBSCRIBE,
        ZADD,
        ZRANK,
        ZRANGE,
        ZCARD,
        ZSCORE,
        ZREM
    };

    // Base command structure
    struct Command
    {
        CommandType type{};
        std::string key{};
        size_t bytes_processed{0}; // Bytes consumed from read_buffer when parsing this command
        virtual ~Command() = default;
        virtual bool is_write_command() const = 0;
        virtual std::string to_resp() const = 0;
    };

    struct UnSubscribeCommand: public Command {
        std::string channel_name;
        UnSubscribeCommand() {type = CommandType::UNSUBSCRIBE;}
        bool is_write_command() const override{
            return false;
        }
        std::string to_resp() const override {
            return "";
        }

    };

    struct ZaddCommand : public Command
    {
        ZaddCommand() { type = CommandType::ZADD; }
        std::vector<std::pair<double, std::string>> score_members{};
        bool is_write_command() const override
        {
            return true;
        }
        std::string to_resp() const override
        {
            std::string cmd = "*" + std::to_string(2 + score_members.size() * 2) + "\r\n";
            cmd += "$4\r\nZADD\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            for (const auto &sm : score_members)
            {
                std::string score_str = std::to_string(sm.first);
                cmd += "$" + std::to_string(score_str.length()) + "\r\n" + score_str + "\r\n";
                cmd += "$" + std::to_string(sm.second.length()) + "\r\n" + sm.second + "\r\n";
            }
            return cmd;
        }
    };

    struct SubscribeCommand : public Command
    {
        std::string channel_name;
        SubscribeCommand() { type = CommandType::SUBSCRIBE; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            return "";
        }
    };
    struct PublishCommand: public Command {
        std::string channel_name;
        std::string message;
        PublishCommand() {type = CommandType::PUBLISH;}

        bool is_write_command() const override {
            return false;
        }
        std::string to_resp() const override {
            return "";
        }
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
            return "*1\r\n$4\r\nPING\r\n";
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
            std::string cmd = "*2\r\n$4\r\nECHO\r\n";
            cmd += "$" + std::to_string(message.length()) + "\r\n" + message + "\r\n";
            return cmd;
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
            std::string cmd = "*2\r\n$3\r\nGET\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            return cmd;
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
            std::string cmd = "*2\r\n$4\r\nTYPE\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            return cmd;
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
            std::string cmd = "*2\r\n$4\r\nINCR\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            return cmd;
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
            std::string cmd = "*" + std::to_string(2 + values.size()) + "\r\n";
            cmd += "$5\r\nRPUSH\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            for (const auto &val : values)
            {
                cmd += "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
            }
            return cmd;
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
            std::string cmd = "*" + std::to_string(2 + values.size()) + "\r\n";
            cmd += "$5\r\nLPUSH\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            for (const auto &val : values)
            {
                cmd += "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
            }
            return cmd;
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
            std::string cmd = "*4\r\n$6\r\nLRANGE\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            std::string start_str = std::to_string(start);
            std::string stop_str = std::to_string(stop);
            cmd += "$" + std::to_string(start_str.length()) + "\r\n" + start_str + "\r\n";
            cmd += "$" + std::to_string(stop_str.length()) + "\r\n" + stop_str + "\r\n";
            return cmd;
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
            std::string cmd = "*2\r\n$4\r\nLLEN\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            return cmd;
        }
    };

    // LPOP command with optional count parameter
    struct LpopCommand : public Command
    {
        LpopCommand() { type = CommandType::LPOP; }
        std::optional<uint32_t> count{}; // Optional count parameter
        bool is_write_command() const override
        {
            return true;
        }
        std::string to_resp() const override
        {
            int arg_count = count.has_value() ? 3 : 2;
            std::string cmd = "*" + std::to_string(arg_count) + "\r\n$4\r\nLPOP\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            if (count.has_value())
            {
                std::string count_str = std::to_string(*count);
                cmd += "$" + std::to_string(count_str.length()) + "\r\n" + count_str + "\r\n";
            }
            return cmd;
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
            std::string cmd = "*" + std::to_string(1 + keys.size() + 1) + "\r\n";
            cmd += "$5\r\nBLPOP\r\n";
            for (const auto &k : keys)
            {
                cmd += "$" + std::to_string(k.length()) + "\r\n" + k + "\r\n";
            }
            std::string timeout_str = std::to_string(static_cast<int64_t>(timeout));
            cmd += "$" + std::to_string(timeout_str.length()) + "\r\n" + timeout_str + "\r\n";
            return cmd;
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
            return true;
        }
        std::string to_resp() const override
        {
            // XADD key id field value [field value ...]
            std::string cmd = "*" + std::to_string(3 + field_values.size() * 2) + "\r\n";
            cmd += "$4\r\nXADD\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            cmd += "$" + std::to_string(stream_id.length()) + "\r\n" + stream_id + "\r\n";
            for (const auto &fv : field_values)
            {
                cmd += "$" + std::to_string(fv.first.length()) + "\r\n" + fv.first + "\r\n";
                cmd += "$" + std::to_string(fv.second.length()) + "\r\n" + fv.second + "\r\n";
            }
            return cmd;
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
            std::string cmd = "*4\r\n$6\r\nXRANGE\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            cmd += "$" + std::to_string(start.length()) + "\r\n" + start + "\r\n";
            cmd += "$" + std::to_string(end.length()) + "\r\n" + end + "\r\n";
            return cmd;
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
            // XREAD [BLOCK milliseconds] STREAMS key [key ...] id [id ...]
            int arg_count = 2 + keys.size() + ids.size(); // XREAD + STREAMS + keys + ids
            if (block_timeout.has_value())
                arg_count += 2; // BLOCK + timeout
            std::string cmd = "*" + std::to_string(arg_count) + "\r\n";
            cmd += "$5\r\nXREAD\r\n";
            if (block_timeout.has_value())
            {
                cmd += "$5\r\nBLOCK\r\n";
                std::string timeout_str = std::to_string(static_cast<int64_t>(*block_timeout));
                cmd += "$" + std::to_string(timeout_str.length()) + "\r\n" + timeout_str + "\r\n";
            }
            cmd += "$7\r\nSTREAMS\r\n";
            for (const auto &k : keys)
            {
                cmd += "$" + std::to_string(k.length()) + "\r\n" + k + "\r\n";
            }
            for (const auto &id : ids)
            {
                cmd += "$" + std::to_string(id.length()) + "\r\n" + id + "\r\n";
            }
            return cmd;
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
            return "*1\r\n$5\r\nMULTI\r\n";
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
            return "*1\r\n$4\r\nEXEC\r\n";
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
            return "*1\r\n$7\r\nDISCARD\r\n";
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
            if (isReplicationArgument)
            {
                return "*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n";
            }
            return "*1\r\n$4\r\nINFO\r\n";
        }
    };
    enum class ReplConfType
    {
        LISTENING_PORT,
        CAPA,
        GETACK, // Master asking slave for ACK
        ACK     // Slave responding with offset
    };

    struct ReplConfCommand : public Command
    {
        ReplConfType subcommand{ReplConfType::LISTENING_PORT};
        std::string listening_port;
        std::string capability;
        int64_t ack_offset{0}; // Used for ACK response

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
            if (subcommand == ReplConfType::ACK)
            {
                std::string offset_str = std::to_string(ack_offset);
                std::string cmd = "*3\r\n$8\r\nREPLCONF\r\n$3\r\nACK\r\n";
                cmd += "$" + std::to_string(offset_str.length()) + "\r\n" + offset_str + "\r\n";
                return cmd;
            }
            if (subcommand == ReplConfType::GETACK)
            {
                return "*3\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n$1\r\n*\r\n";
            }
            std::string cmd = "*3\r\n$8\r\nREPLCONF\r\n";
            if (!listening_port.empty())
            {
                cmd += "$14\r\nlistening-port\r\n";
                cmd += "$" + std::to_string(listening_port.length()) + "\r\n" + listening_port + "\r\n";
            }
            else if (!capability.empty())
            {
                cmd += "$4\r\ncapa\r\n";
                cmd += "$" + std::to_string(capability.length()) + "\r\n" + capability + "\r\n";
            }
            return cmd;
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
            std::string cmd = "*3\r\n$5\r\nPSYNC\r\n";
            cmd += "$" + std::to_string(repl_id.length()) + "\r\n" + repl_id + "\r\n";
            std::string offset_str = std::to_string(offset);
            cmd += "$" + std::to_string(offset_str.length()) + "\r\n" + offset_str + "\r\n";
            return cmd;
        }
    };
    struct WaitCommand : public Command
    {
        unsigned long long num_replica{0};
        unsigned long long timeout;
        WaitCommand()
        {
            type = CommandType::WAIT;
        }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            std::string num_replica_in_string = std::to_string(num_replica);
            std::string timeout_in_string = std::to_string(timeout);
            std::string cmd = "*3\r\n$4\r\nWAIT\r\n";
            cmd += "$" + std::to_string(num_replica_in_string.length()) + "\r\n" + num_replica_in_string + "\r\n";
            cmd += "$" + std::to_string(timeout_in_string.length()) + "\r\n" + timeout_in_string + "\r\n";
            return cmd;
        }
    };
    struct GetConfigCommand : public Command
    {
        std::vector<std::string> parameters{};
        GetConfigCommand()
        {
            type = CommandType::GETCONFIG;
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

    struct KeysCommand : public Command
    {
        std::string pattern{};

        KeysCommand()
        {
            type = CommandType::KEYS;
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

    struct ZrankCommand : public Command
    {
        ZrankCommand() { type = CommandType::ZRANK; }
        std::string member{};
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            std::string cmd = "*3\r\n$5\r\nZRANK\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            cmd += "$" + std::to_string(member.length()) + "\r\n" + member + "\r\n";
            return cmd;
        }
    };

    struct ZrangeCommand : public Command
    {
        ZrangeCommand() { type = CommandType::ZRANGE; }
        int64_t start{};
        int64_t stop{};
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            std::string cmd = "*4\r\n$6\r\nZRANGE\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            std::string start_str = std::to_string(start);
            std::string stop_str = std::to_string(stop);
            cmd += "$" + std::to_string(start_str.length()) + "\r\n" + start_str + "\r\n";
            cmd += "$" + std::to_string(stop_str.length()) + "\r\n" + stop_str + "\r\n";
            return cmd;
        }
    };

    struct ZcardCommand : public Command
    {
        ZcardCommand() { type = CommandType::ZCARD; }
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            std::string cmd = "*2\r\n$5\r\nZCARD\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            return cmd;
        }
    };

    struct ZscoreCommand : public Command
    {
        ZscoreCommand() { type = CommandType::ZSCORE; }
        std::string member{};
        bool is_write_command() const override
        {
            return false;
        }
        std::string to_resp() const override
        {
            std::string cmd = "*3\r\n$6\r\nZSCORE\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            cmd += "$" + std::to_string(member.length()) + "\r\n" + member + "\r\n";
            return cmd;
        }
    };

    struct ZremCommand : public Command
    {
        ZremCommand() { type = CommandType::ZREM; }
        std::vector<std::string> members{};
        bool is_write_command() const override
        {
            return true;
        }
        std::string to_resp() const override
        {
            std::string cmd = "*" + std::to_string(2 + members.size()) + "\r\n";
            cmd += "$4\r\nZREM\r\n";
            cmd += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
            for (const auto &member : members)
            {
                cmd += "$" + std::to_string(member.length()) + "\r\n" + member + "\r\n";
            }
            return cmd;
        }
    };

    inline std::string commandTypeToString(CommandType type)
    {
        switch (type)
        {
        case CommandType::ECHO:
            return "echo";
        case CommandType::PING:
            return "ping";
        case CommandType::SET:
            return "set";
        case CommandType::GET:
            return "get";
        case CommandType::TYPE:
            return "type";
        case CommandType::RPUSH:
            return "rpush";
        case CommandType::LPUSH:
            return "lpush";
        case CommandType::LRANGE:
            return "lrange";
        case CommandType::LLEN:
            return "llen";
        case CommandType::LPOP:
            return "lpop";
        case CommandType::BLPOP:
            return "blpop";
        case CommandType::XADD:
            return "xadd";
        case CommandType::XRANGE:
            return "xrange";
        case CommandType::XREAD:
            return "xread";
        case CommandType::INCR:
            return "incr";
        case CommandType::UNKNOWN:
            return "unknown";
        case CommandType::MULTI:
            return "multi";
        case CommandType::EXEC:
            return "exec";
        case CommandType::DISCARD:
            return "discard";
        case CommandType::INFO:
            return "info";
        case CommandType::REPLCONF:
            return "replconf";
        case CommandType::PSYNC:
            return "psync";
        case CommandType::WAIT:
            return "wait";
        case CommandType::GETCONFIG:
            return "getconfig";
        case CommandType::KEYS:
            return "keys";
        case CommandType::SUBSCRIBE:
            return "subscribe";
        case CommandType::UNSUBSCRIBE:
            return "unsubscribe";
        case CommandType::ZADD:
            return "zadd";
        case CommandType::ZRANK:
            return "zrank";
        case CommandType::ZRANGE:
            return "zrange";
        case CommandType::ZCARD:
            return "zcard";
        case CommandType::ZSCORE:
            return "zscore";
        case CommandType::ZREM:
            return "zrem";
        default:
            return "unknown";
        }
    }
}