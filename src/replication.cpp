#include "replication.h"
namespace REPLICATION_NAMESPACE
{
    void MasterClient::handle_master(bool is_read_event,bool &should_invert)
    {
        if (!is_handshake_completed)
        {
            handle_handshake(is_read_event,should_invert);
        }
    }
    void MasterClient::handle_handshake(bool is_read_event,bool &should_invert)
    {
        if (is_read_event)
        {
            NETWORKING::read_client(fd,read_buffer);
            std::string response;
            bool status = Parser::parse_simple_string(read_buffer, response);
            if (!status)
                return;
            if (replication_status == ReplicationStatus::PING_SENT && is_equal(response, "PONG"))
                replication_status = ReplicationStatus::PING_SENT_SUCCESS;
            else if (replication_status == ReplicationStatus::REPLCONF_PORT_SENT && is_equal(response, "OK"))
                replication_status = ReplicationStatus::REPLCONF_PORT_SUCCESS;
            else if (replication_status == ReplicationStatus::REPLCONF_CAPA_SENT && is_equal(response, "OK"))
            {
                replication_status = ReplicationStatus::REPLCONF_CAPA_SUCCESS;
            }
            else if(replication_status==ReplicationStatus::PSYNC_SENT)
            {   replication_status == ReplicationStatus::HANDSHAKE_SUCCESS;
                is_handshake_completed = true;
            }
            read_buffer.clear();
            should_invert = true;
            
        }
        if (!is_read_event)
        {if (replication_status == ReplicationStatus::IDLE)
            {
                // send ping handshake here
                encode_array_header(&write_buffer, 1);
                encode_bulk_string(&write_buffer, "PING");
                replication_status = ReplicationStatus::PING_SENT;
            }
            if (replication_status == ReplicationStatus::PING_SENT_SUCCESS)
            {
                encode_array_header(&write_buffer, 3);
                encode_bulk_string(&write_buffer, "REPLCONF");
                encode_bulk_string(&write_buffer, "listening-port");
                encode_bulk_string(&write_buffer, std::to_string(port));
                replication_status = ReplicationStatus::REPLCONF_PORT_SENT;
            }
            if (replication_status == ReplicationStatus::REPLCONF_PORT_SUCCESS)
            {
                encode_array_header(&write_buffer, 3);
                encode_bulk_string(&write_buffer, "REPLCONF");
                encode_bulk_string(&write_buffer, "capa");
                encode_bulk_string(&write_buffer, "psync2");
                replication_status = ReplicationStatus::REPLCONF_CAPA_SENT;
            }
            if(replication_status == ReplicationStatus::REPLCONF_CAPA_SUCCESS)
            {
                encode_array_header(&write_buffer,3);
                encode_bulk_string(&write_buffer,"PSYNC");
                encode_bulk_string(&write_buffer,repl_id);
                encode_bulk_string(&write_buffer,std::to_string(offset));
                replication_status = ReplicationStatus::PSYNC_SENT;
            }
            NETWORKING::write_client(fd,write_buffer);
            should_invert = true;
        }
        
    }

}
