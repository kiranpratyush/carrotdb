#pragma once
#include "models/client.h"
#include "models/server-model.h"
#include "models/IEventLoop.h"
#include "utils/utils.h"
#include "parser/parser.h"
#include "db/db.h"

using namespace SERVER_NAMESPACE;
using namespace REDIS_NAMESPACE;

namespace SLAVE_REPLICATION_NAMESPACE
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

    class SlaveReplicationClient : public Client
    {
    private:
        ReplicationStatus replication_status{ReplicationStatus::IDLE};
        uint port{6380};
        bool is_handshake_completed{false};
        std::string repl_id{"?"};
        int64_t offset{-1};
        int64_t bytes_processed{0};
        DB *db{nullptr};
        ServerConfig *config{nullptr};

        void handle_handshake_read();
        void process_commands();

    public:
        void prepare_handshake_write();
        void process_read_buffer();
        void set_db_and_config(DB *database, ServerConfig *server_config);
        bool has_data_to_write() const { return !write_buffer.empty(); }

        inline void setrepl_id(std::string_view id) { repl_id = id; }
        inline std::string getrepl_id() { return repl_id; }
        inline void setoffset(int64_t off) { offset = off; }
        inline int64_t getOffset() { return offset; }
        inline int64_t getBytesProcessed() const { return bytes_processed; }

        SlaveReplicationClient(uint p) : port{p} {}
    };
}
