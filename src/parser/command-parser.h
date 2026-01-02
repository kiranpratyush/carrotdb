#pragma once
#include "parser/command.h"
#include "models/client.h"

using namespace SERVER_NAMESPACE;

namespace REDIS_NAMESPACE
{
    class CommandParser
    {
    public:
        static Command parseCommand(ClientContext &c, int total_commands);

    private:
        static Command parsePingCommand(ClientContext &c);
        static Command parseEchoCommand(ClientContext &c);
        static Command parseSetCommand(ClientContext &c, int total_commands);
        static Command parseGetCommand(ClientContext &c);
        static Command parseTypeCommand(ClientContext &c);
        static Command parseIncrCommand(ClientContext &c);
        static Command parseRpushCommand(ClientContext &c, int total_commands);
        static Command parseLpushCommand(ClientContext &c, int total_commands);
        static Command parseLrangeCommand(ClientContext &c);
        static Command parseLlenCommand(ClientContext &c);
        static Command parseLpopCommand(ClientContext &c, int total_commands);
        static Command parseBlpopCommand(ClientContext &c, int total_commands);
        static Command parseXaddCommand(ClientContext &c, int total_commands);
        static Command parseXrangeCommand(ClientContext &c);
        static Command parseXreadCommand(ClientContext &c, int total_commands);
    };
}