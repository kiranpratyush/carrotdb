#include "parser/command-parser.h"
#include "parser/parser.h"
#include "models/resp.h"
#include "utils/utils.h"
#include "iostream"
#include <memory>

namespace REDIS_NAMESPACE
{
    std::unique_ptr<Command> CommandParser::parseCommand(ClientContext &c)
    {
        size_t start_position = c.current_read_position;

        ParsedToken t = Parser::Parse(c);

        if (t.type != ParsedToken::Type::ARRAY)
        {
            std::string &buffer = c.client->read_buffer;
            size_t next_array = buffer.find('*', c.current_read_position);
            if (next_array != std::string::npos && next_array > 0)
            {
                std::cout << "[parseCommand] Skipping " << next_array << " bytes of non-RESP data (possibly RDB)" << std::endl;
                buffer.erase(0, next_array);
                c.current_read_position = 0;
                // Try parsing again after cleanup
                return parseCommand(c);
            }
            return std::make_unique<UnknowCommand>();
        }
        auto total_commands = t.length;
        t = Parser::Parse(c);
        std::string_view cmd_name{c.client->read_buffer.data() + t.start_pos, t.end_pos - t.start_pos + 1};
        bool isParsedUnknown = false;
        std::unique_ptr<Command> cmd;
        if (is_equal(cmd_name, "PING"))
            cmd = parsePingCommand(c);
        else if (is_equal(cmd_name, "ECHO"))
            cmd = parseEchoCommand(c);
        else if (is_equal(cmd_name, "SET"))
            cmd = parseSetCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "GET"))
            cmd = parseGetCommand(c);
        else if (is_equal(cmd_name, "TYPE"))
            cmd = parseTypeCommand(c);
        else if (is_equal(cmd_name, "INCR"))
            cmd = parseIncrCommand(c);
        else if (is_equal(cmd_name, "RPUSH"))
            cmd = parseRpushCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "LPUSH"))
            cmd = parseLpushCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "LRANGE"))
            cmd = parseLrangeCommand(c);
        else if (is_equal(cmd_name, "LLEN"))
            cmd = parseLlenCommand(c);
        else if (is_equal(cmd_name, "LPOP"))
            cmd = parseLpopCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "BLPOP"))
            cmd = parseBlpopCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "XADD"))
            cmd = parseXaddCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "XRANGE"))
            cmd = parseXrangeCommand(c);
        else if (is_equal(cmd_name, "XREAD"))
            cmd = parseXreadCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "MULTI"))
            cmd = parseMultiCommand(c);
        else if (is_equal(cmd_name, "EXEC"))
            cmd = parseExecCommand(c);
        else if (is_equal(cmd_name, "DISCARD"))
            cmd = parseDiscardCommand(c);
        else if (is_equal(cmd_name, "INFO"))
            cmd = parseInfoCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "REPLCONF"))
            cmd = parseReplConfCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "PSYNC"))
            cmd = parsePsyncCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "WAIT"))
            cmd = parseWaitCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "CONFIG"))
            cmd = parseGetConfigCommand(c, total_commands - 1);
        else if (is_equal(cmd_name, "KEYS"))
            cmd = parseKeysCommand(c, total_commands - 1);
        else
        {
            cmd = std::make_unique<UnknowCommand>();
            isParsedUnknown = true;
        }

        // Track bytes processed and clean up the read buffer
        size_t bytes_parsed = c.current_read_position - start_position;
        cmd->bytes_processed = bytes_parsed;
        c.command_bytes = bytes_parsed;

        // Clean up the parsed portion from read buffer
        if (isParsedUnknown)
            c.client->read_buffer.clear();
        else
            c.client->read_buffer.erase(0, bytes_parsed);
        // Reset position since we cleaned the buffer
        c.current_read_position = 0;

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parsePingCommand(ClientContext &c)
    {
        return std::make_unique<PingCommand>();
    }

    std::unique_ptr<Command> CommandParser::parseEchoCommand(ClientContext &c)
    {
        ParsedToken message_token = Parser::Parse(c);
        if (message_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();
        auto cmd = std::make_unique<EchoCommand>();
        cmd->message = std::string{c.client->read_buffer.data() + message_token.start_pos, message_token.end_pos - message_token.start_pos + 1};
        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseSetCommand(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        ParsedToken value_token = Parser::Parse(c);
        if (value_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<SetCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};
        cmd->value = std::string{read_buffer.data() + value_token.start_pos,
                                 value_token.end_pos - value_token.start_pos + 1};

        total_commands -= 2; // key and value consumed

        // Parse optional EX and PX flags
        while (total_commands > 0)
        {
            ParsedToken flag_token = Parser::Parse(c);
            if (flag_token.type != ParsedToken::Type::BULK_STRING)
                break;

            std::string_view flag{read_buffer.data() + flag_token.start_pos,
                                  flag_token.end_pos - flag_token.start_pos + 1};

            if (is_equal(flag, "EX") && total_commands > 1)
            {
                ParsedToken ex_value_token = Parser::Parse(c);
                if (ex_value_token.type == ParsedToken::Type::BULK_STRING)
                {
                    std::string_view ex_str{read_buffer.data() + ex_value_token.start_pos,
                                            ex_value_token.end_pos - ex_value_token.start_pos + 1};
                    unsigned long long ex_val;
                    if (convert_positive_string_to_number(ex_str, ex_val))
                        cmd->ex = ex_val;
                }
                total_commands -= 2;
            }
            else if (is_equal(flag, "PX") && total_commands > 1)
            {
                ParsedToken px_value_token = Parser::Parse(c);
                if (px_value_token.type == ParsedToken::Type::BULK_STRING)
                {
                    std::string_view px_str{read_buffer.data() + px_value_token.start_pos,
                                            px_value_token.end_pos - px_value_token.start_pos + 1};
                    unsigned long long px_val;
                    if (convert_positive_string_to_number(px_str, px_val))
                        cmd->px = px_val;
                }
                total_commands -= 2;
            }
            else
            {
                total_commands--;
            }
        }

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseGetCommand(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<GetCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};
        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseTypeCommand(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<TypeCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};
        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseIncrCommand(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<IncrCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};
        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseRpushCommand(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<RpushCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};

        total_commands--; // key consumed

        // Parse all values
        while (total_commands > 0)
        {
            ParsedToken value_token = Parser::Parse(c);
            if (value_token.type == ParsedToken::Type::BULK_STRING)
            {
                cmd->values.push_back(std::string{read_buffer.data() + value_token.start_pos,
                                                  value_token.end_pos - value_token.start_pos + 1});
            }
            total_commands--;
        }

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseLpushCommand(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<LpushCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};

        total_commands--; // key consumed

        // Parse all values
        while (total_commands > 0)
        {
            ParsedToken value_token = Parser::Parse(c);
            if (value_token.type == ParsedToken::Type::BULK_STRING)
            {
                cmd->values.push_back(std::string{read_buffer.data() + value_token.start_pos,
                                                  value_token.end_pos - value_token.start_pos + 1});
            }
            total_commands--;
        }

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseLrangeCommand(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        ParsedToken start_token = Parser::Parse(c);
        if (start_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        ParsedToken stop_token = Parser::Parse(c);
        if (stop_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<LrangeCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};

        std::string_view start_str{read_buffer.data() + start_token.start_pos,
                                   start_token.end_pos - start_token.start_pos + 1};
        std::string_view stop_str{read_buffer.data() + stop_token.start_pos,
                                  stop_token.end_pos - stop_token.start_pos + 1};

        if (!convert_string_to_number(start_str, &cmd->start) ||
            !convert_string_to_number(stop_str, &cmd->stop))
            return std::make_unique<UnknowCommand>();

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseLlenCommand(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<LlenCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};
        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseLpopCommand(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<LpopCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};

        total_commands--; // key consumed

        // Parse optional count parameter
        if (total_commands > 0)
        {
            ParsedToken count_token = Parser::Parse(c);
            if (count_token.type == ParsedToken::Type::BULK_STRING)
            {
                std::string_view count_str{read_buffer.data() + count_token.start_pos,
                                           count_token.end_pos - count_token.start_pos + 1};
                unsigned long long count_val;
                if (convert_positive_string_to_number(count_str, count_val))
                    cmd->count = static_cast<uint32_t>(count_val);
            }
        }

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseBlpopCommand(ClientContext &c, int total_commands)
    {
        auto cmd = std::make_unique<BlpopCommand>();
        std::string_view read_buffer = c.client->read_buffer;

        // Parse keys (all except the last parameter which is timeout)
        while (total_commands > 1)
        {
            ParsedToken key_token = Parser::Parse(c);
            if (key_token.type == ParsedToken::Type::BULK_STRING)
            {
                cmd->keys.push_back(std::string{read_buffer.data() + key_token.start_pos,
                                                key_token.end_pos - key_token.start_pos + 1});
            }
            total_commands--;
        }

        // Parse timeout
        if (total_commands > 0)
        {
            ParsedToken timeout_token = Parser::Parse(c);
            if (timeout_token.type == ParsedToken::Type::BULK_STRING)
            {
                std::string timeout_str{read_buffer.data() + timeout_token.start_pos,
                                        timeout_token.end_pos - timeout_token.start_pos + 1};
                try
                {
                    cmd->timeout = std::stod(timeout_str);
                }
                catch (...)
                {
                    cmd->timeout = 0.0;
                }
            }
        }

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseXaddCommand(ClientContext &c, int total_commands)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        ParsedToken id_token = Parser::Parse(c);
        if (id_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<XaddCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};
        cmd->stream_id = std::string{read_buffer.data() + id_token.start_pos,
                                     id_token.end_pos - id_token.start_pos + 1};

        total_commands -= 2; // key and id consumed

        // Parse field-value pairs
        while (total_commands > 1)
        {
            ParsedToken field_token = Parser::Parse(c);
            ParsedToken value_token = Parser::Parse(c);

            if (field_token.type == ParsedToken::Type::BULK_STRING &&
                value_token.type == ParsedToken::Type::BULK_STRING)
            {
                std::string field{read_buffer.data() + field_token.start_pos,
                                  field_token.end_pos - field_token.start_pos + 1};
                std::string value{read_buffer.data() + value_token.start_pos,
                                  value_token.end_pos - value_token.start_pos + 1};
                cmd->field_values.push_back({field, value});
            }
            total_commands -= 2;
        }

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseXrangeCommand(ClientContext &c)
    {
        ParsedToken key_token = Parser::Parse(c);
        if (key_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        ParsedToken start_token = Parser::Parse(c);
        if (start_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        ParsedToken end_token = Parser::Parse(c);
        if (end_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view read_buffer = c.client->read_buffer;
        auto cmd = std::make_unique<XrangeCommand>();
        cmd->key = std::string{read_buffer.data() + key_token.start_pos,
                               key_token.end_pos - key_token.start_pos + 1};
        cmd->start = std::string{read_buffer.data() + start_token.start_pos,
                                 start_token.end_pos - start_token.start_pos + 1};
        cmd->end = std::string{read_buffer.data() + end_token.start_pos,
                               end_token.end_pos - end_token.start_pos + 1};
        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseXreadCommand(ClientContext &c, int total_commands)
    {
        auto cmd = std::make_unique<XreadCommand>();
        std::string_view read_buffer = c.client->read_buffer;

        // Check for BLOCK flag
        ParsedToken first_token = Parser::Parse(c);
        if (first_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        std::string_view first_arg{read_buffer.data() + first_token.start_pos,
                                   first_token.end_pos - first_token.start_pos + 1};

        if (is_equal(first_arg, "BLOCK"))
        {
            ParsedToken timeout_token = Parser::Parse(c);
            if (timeout_token.type == ParsedToken::Type::BULK_STRING)
            {
                std::string timeout_str{read_buffer.data() + timeout_token.start_pos,
                                        timeout_token.end_pos - timeout_token.start_pos + 1};
                try
                {
                    cmd->block_timeout = std::stod(timeout_str);
                }
                catch (...)
                {
                    cmd->block_timeout = 0.0;
                }
            }
            total_commands -= 2; // BLOCK and timeout consumed

            // Parse STREAMS keyword
            ParsedToken streams_token = Parser::Parse(c);
            total_commands--;
        }
        else if (is_equal(first_arg, "STREAMS"))
        {
            total_commands--; // STREAMS consumed
        }
        else
        {
            return std::make_unique<UnknowCommand>();
        }

        // Parse keys and IDs (split evenly)
        if (total_commands % 2 != 0)
            return std::make_unique<UnknowCommand>();

        int num_streams = total_commands / 2;

        // Parse keys
        for (int i = 0; i < num_streams; i++)
        {
            ParsedToken key_token = Parser::Parse(c);
            if (key_token.type == ParsedToken::Type::BULK_STRING)
            {
                cmd->keys.push_back(std::string{read_buffer.data() + key_token.start_pos,
                                                key_token.end_pos - key_token.start_pos + 1});
            }
        }

        // Parse IDs
        for (int i = 0; i < num_streams; i++)
        {
            ParsedToken id_token = Parser::Parse(c);
            if (id_token.type == ParsedToken::Type::BULK_STRING)
            {
                cmd->ids.push_back(std::string{read_buffer.data() + id_token.start_pos,
                                               id_token.end_pos - id_token.start_pos + 1});
            }
        }

        return cmd;
    }
    std::unique_ptr<Command> CommandParser::parseMultiCommand(ClientContext &c)
    {
        auto cmd = std::make_unique<MultiCommand>();
        return cmd;
    }
    std::unique_ptr<Command> CommandParser::parseExecCommand(ClientContext &c)
    {
        auto cmd = std::make_unique<ExecCommand>();
        return cmd;
    }
    std::unique_ptr<Command> CommandParser::parseDiscardCommand(ClientContext &c)
    {
        auto cmd = std::make_unique<DiscardCommand>();
        return cmd;
    }
    std::unique_ptr<Command> CommandParser::parseInfoCommand(ClientContext &c, int total_commands)
    {
        ParsedToken info_token = Parser::Parse(c);
        if (info_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        auto cmd = std::make_unique<InfoCommand>();
        std::string_view info_section{c.client->read_buffer.data() + info_token.start_pos,
                                      info_token.end_pos - info_token.start_pos + 1};
        if (is_equal(info_section, "replication"))
        {
            cmd->isReplicationArgument = true;
        }

        return cmd;
    }
    std::unique_ptr<Command> CommandParser::parseReplConfCommand(ClientContext &c, int total_commands)
    {
        ParsedToken type = Parser::Parse(c);
        if (type.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();
        auto cmd = std::make_unique<ReplConfCommand>();
        std::string_view confType{c.client->read_buffer.data() + type.start_pos, type.end_pos - type.start_pos + 1};
        if (is_equal(confType, "listening-port"))
        {
            cmd->subcommand = ReplConfType::LISTENING_PORT;
            unsigned long long portInNumber{};
            ParsedToken portToken = Parser::Parse(c);
            std::string_view port{c.client->read_buffer.data() + portToken.start_pos, portToken.end_pos - portToken.start_pos + 1};
            bool is_success = convert_positive_string_to_number(port, portInNumber);
            if (is_success)
            {
                cmd->listening_port = portInNumber;
            }
        }
        else if (is_equal(confType, "capa"))
        {
            cmd->subcommand = ReplConfType::CAPA;
            ParsedToken capaToken = Parser::Parse(c);
            std::string_view capa{c.client->read_buffer.data() + capaToken.start_pos, capaToken.end_pos - capaToken.start_pos + 1};
            cmd->capability = capa;
        }
        else if (is_equal(confType, "GETACK"))
        {
            cmd->subcommand = ReplConfType::GETACK;
            // Parse the "*" argument (we don't really need it)
            Parser::Parse(c);
        }
        else if (is_equal(confType, "ACK"))
        {
            cmd->subcommand = ReplConfType::ACK;
            ParsedToken offsetToken = Parser::Parse(c);
            std::string_view offset_str{c.client->read_buffer.data() + offsetToken.start_pos, offsetToken.end_pos - offsetToken.start_pos + 1};
            unsigned long long offset_val{};
            if (convert_positive_string_to_number(offset_str, offset_val))
            {
                cmd->ack_offset = static_cast<int64_t>(offset_val);
            }
        }
        return cmd;
    }
    std::unique_ptr<Command> CommandParser::parsePsyncCommand(ClientContext &c, int total_commands)
    {
        auto cmd = std::make_unique<PsyncCommand>();

        // Parse replication ID
        ParsedToken repl_id_token = Parser::Parse(c);
        if (repl_id_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();

        cmd->repl_id = std::string{c.client->read_buffer.data() + repl_id_token.start_pos,
                                   repl_id_token.end_pos - repl_id_token.start_pos + 1};

        // Parse offset
        if (total_commands > 1)
        {
            ParsedToken offset_token = Parser::Parse(c);
            if (offset_token.type != ParsedToken::Type::BULK_STRING)
                return std::make_unique<UnknowCommand>();

            std::string_view offset_str{c.client->read_buffer.data() + offset_token.start_pos,
                                        offset_token.end_pos - offset_token.start_pos + 1};

            // Handle both positive numbers and -1
            if (offset_str == "-1")
            {
                cmd->offset = -1;
            }
            else
            {
                unsigned long long temp_offset;
                if (convert_positive_string_to_number(offset_str, temp_offset))
                {
                    cmd->offset = static_cast<int64_t>(temp_offset);
                }
                else
                {
                    cmd->offset = -1;
                }
            }
        }

        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseWaitCommand(ClientContext &c, int total_commands)
    {
        ParsedToken num_replica_token = Parser::Parse(c);
        if (num_replica_token.type != ParsedToken::Type::BULK_STRING)
            return std::make_unique<UnknowCommand>();
        std::string_view num_replica{c.client->read_buffer.data() + num_replica_token.start_pos, num_replica_token.end_pos - num_replica_token.start_pos + 1};
        auto cmd = std::make_unique<WaitCommand>();
        convert_positive_string_to_number(num_replica, cmd->num_replica);
        ParsedToken timeout_token = Parser::Parse(c);
        std::string_view timeout_ms_in_string{c.client->read_buffer.data() + timeout_token.start_pos, timeout_token.end_pos - timeout_token.start_pos + 1};
        convert_positive_string_to_number(timeout_ms_in_string, cmd->timeout);
        return cmd;
    }

    std::unique_ptr<Command> CommandParser::parseGetConfigCommand(ClientContext &c, int total_commands)
    {
        auto unknownCommand = std::make_unique<UnknowCommand>();
        auto getConfigCommand = std::make_unique<GetConfigCommand>();
        ParsedToken configTypeToken = Parser::Parse(c);
        if (configTypeToken.type != ParsedToken::Type::BULK_STRING)
            return unknownCommand;
        std::string_view commandType{c.client->read_buffer.data() + configTypeToken.start_pos, configTypeToken.end_pos - configTypeToken.start_pos + 1};
        if (!is_equal(commandType, "GET"))
            return unknownCommand;
        total_commands--;
        while (total_commands > 0)
        {
            ParsedToken parameterToken = Parser::Parse(c);
            if (parameterToken.type != ParsedToken::Type::BULK_STRING)
                return unknownCommand;
            std::string_view parameterType{c.client->read_buffer.data() + parameterToken.start_pos, parameterToken.end_pos - parameterToken.start_pos + 1};
            getConfigCommand->parameters.push_back(std::string(parameterType));
            total_commands--;
        }
        return getConfigCommand;
    }

    std::unique_ptr<Command> CommandParser::parseKeysCommand(ClientContext &c, int total_commands)
    {
        auto unknownCommand = std::make_unique<UnknowCommand>();
        auto keysCommand = std::make_unique<KeysCommand>();
        ParsedToken parameterToken = Parser::Parse(c);
        if (parameterToken.type != ParsedToken::Type::BULK_STRING)
            return unknownCommand;
        std::string_view parameter{c.client->read_buffer.data() + parameterToken.start_pos, parameterToken.end_pos - parameterToken.start_pos + 1};
        keysCommand->pattern = parameter;
        return keysCommand;
    }

}