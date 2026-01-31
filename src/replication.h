#pragma once
#include <functional>
#include "models/client.h"
#include "models/server-model.h"
#include "utils/utils.h"
#include "parser/parser.h"
#include "networking.h"
#include "db/db.h"

using namespace SERVER_NAMESPACE;
using namespace REDIS_NAMESPACE;

namespace REPLICATION_NAMESPACE
{

    enum class ReplicationStatus
    {
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
    class MasterClient : public Client
    {
    private:
        ReplicationStatus replication_status{ReplicationStatus::IDLE};
        uint port{6380};
        bool is_handshake_completed{false};
        std::string repl_id{"?"};
        int64_t offset{-1};
        int64_t bytes_processed{0};  // Track bytes received from master for ACK
        DB *db{nullptr};
        ServerConfig *config{nullptr};
        void handle_handshake(bool is_read_event, bool &should_invert);
        bool handle_commands();

    public:
        void handle_master(bool is_read_event, bool &should_invert);
        void set_db_and_config(DB *database, ServerConfig *server_config);
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
        inline int64_t getBytesProcessed() const
        {
            return bytes_processed;
        }
        inline void addBytesProcessed(int64_t bytes)
        {
            bytes_processed += bytes;
        }
        MasterClient(uint p) : port{p} {};
    };

    class ReplicationManager
    {
    private:
        std::vector<std::weak_ptr<Client>> slave_clients;
        void handle_info(ClientContext c, ServerConfig &config);
        void handle_replconf(ClientContext c, ServerConfig &config);
        void handle_psync(ClientContext c, ServerConfig &config);
        void handle_wait(ClientContext c, ServerConfig &config);

    public:
        bool handle(ClientContext c, ServerConfig &config);
        bool handle_propagate(ClientContext c);
        void for_each_active_slaves(std::function<void(std::shared_ptr<Client>)> action);
    };

}