#pragma once
#include "models/client.h"
#include "models/server-model.h"
#include "utils/utils.h"
#include "parser/parser.h"
#include "networking.h"

using namespace SERVER_NAMESPACE;

namespace REPLICATION_NAMESPACE
{
    
    enum class ReplicationStatus{
        IDLE,
        PING_SENT,
        PING_SENT_SUCCESS,
        REPLCONF_PORT_SENT,
        REPLCONF_PORT_SUCCESS,
        REPLCONF_CAPA_SENT,
        REPLCONF_CAPA_SUCCESS,
        PSYNC_SENT,
        HANDSHAKE_SUCCESS
    };
    class MasterClient:public Client{
        private:
            ReplicationStatus replication_status{ReplicationStatus::IDLE};
            uint port{6380};
            bool is_handshake_completed{false};
            std::string repl_id{"?"};
            int64_t offset{-1};
            void handle_handshake(bool is_read_event,bool &should_invert);
        public:
            void handle_master(bool is_read_event,bool &should_invert);
            inline void setrepl_id(std::string_view repl_id)
            {
                this->repl_id = repl_id;
            }
            inline std::string getrepl_id()
            {
                return repl_id;
            }
            inline void setoffset(int64_t offset)
            {
                this->offset = offset;
            }
            inline int64_t getOffset()
            {
                return offset;
            }
            MasterClient(uint p):port{p}{};
    };

    class ReplicationManager{
        private:
            void handle_info(ClientContext c,ServerConfig &config);
            void handle_replconf(ClientContext c,ServerConfig &config);
            void handle_psync(ClientContext c,ServerConfig &config);
        public:
            bool handle(ClientContext c,ServerConfig &config);
    };

}