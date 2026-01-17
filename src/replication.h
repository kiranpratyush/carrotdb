#pragma once
#include "models/client.h"
#include "utils/utils.h"
#include "parser/parser.h"
#include "networking.h"

namespace REPLICATION_NAMESPACE
{
    using namespace SERVER_NAMESPACE;

    enum class ReplicationStatus{
        IDLE,
        PING_SENT,
        PING_SENT_SUCCESS,
        REPLCONF_PORT_SENT,
        REPLCONF_PORT_SUCCESS,
        REPLCONF_CAPA_SENT,
        HANDSHAKE_SUCCESS
    };
    class MasterClient:public Client{
        private:
            ReplicationStatus replication_status{ReplicationStatus::IDLE};
            uint port{6380};
            bool is_handshake_completed{false};
            void handle_handshake(bool is_read_event,bool &should_invert);
        public:
            void handle_master(bool is_read_event,bool &should_invert);
            MasterClient(uint p):port{p}{};
    };

}