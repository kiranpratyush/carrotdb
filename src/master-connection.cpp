#include "master-connection.h"
#include "networking.h"
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

using namespace NETWORKING;

namespace MASTER_CONNECTION_NAMESPACE
{
    MasterConnection::MasterConnection(std::string_view host, uint16_t port, uint16_t slave_port,
                                       IEventLoop &_eventloop, DB *_db, ServerConfig *_config)
        : eventloop(_eventloop), db(_db), config(_config), connected(false)
    {
        int sockfd{};
        auto is_success = connect_client(host, port, sockfd);

        if (is_success)
        {
            int flags = fcntl(sockfd, F_GETFL, 0);
            if (flags != -1)
            {
                fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
            }

            slave_client = std::make_unique<SlaveReplicationClient>(slave_port);
            slave_client->fd = sockfd;
            slave_client->set_db_and_config(db, config);
            connected = true;

            eventloop.register_event_handler(this, WRITE_EVENT);

            std::cout << "[SLAVE] Connected to master at " << host << ":" << port << std::endl;

            slave_client->prepare_handshake_write();
        }
        else
        {
            std::cerr << "[SLAVE] Failed to connect to master at " << host << ":" << port << std::endl;
        }
    }

    MasterConnection::~MasterConnection()
    {
        if (connected && slave_client)
        {
            eventloop.remove_event_handler(this);
            close(slave_client->fd);
        }
    }

    void MasterConnection::handleRead()
    {
        if (!slave_client)
            return;

        char temp[1024];
        while (true)
        {
            ssize_t n = read(slave_client->fd, temp, sizeof(temp));
            if (n > 0)
            {
                slave_client->read_buffer.append(temp, n);
            }
            else if (n == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                std::cerr << "[SLAVE] Read error, closing connection" << std::endl;
                handleClose();
                return;
            }
            else
            {
                std::cout << "[SLAVE] Master closed connection" << std::endl;
                handleClose();
                return;
            }
        }

        slave_client->process_read_buffer();

        if (slave_client->has_data_to_write())
        {
            eventloop.update_event_handler(this, WRITE_EVENT);
        }
        else
        {
            eventloop.update_event_handler(this, READ_EVENT);
        }
    }

    void MasterConnection::handleWrite()
    {
        if (!slave_client)
            return;

        std::cout << "[SLAVE] handleWrite" << std::endl;

        if (!slave_client->write_buffer.empty())
        {
            size_t written_total = 0;
            while (written_total < slave_client->write_buffer.size())
            {
                ssize_t n = write(slave_client->fd,
                                  slave_client->write_buffer.data() + written_total,
                                  slave_client->write_buffer.size() - written_total);
                if (n > 0)
                {
                    written_total += n;
                }
                else if (n == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        slave_client->write_buffer.erase(0, written_total);
                        return;
                    }
                    std::cerr << "[SLAVE] Write error" << std::endl;
                    return;
                }
            }
            slave_client->write_buffer.clear();
        }
        eventloop.update_event_handler(this, READ_EVENT);
    }

    void MasterConnection::handleClose()
    {
        if (!connected)
            return;

        std::cout << "[SLAVE] Master connection closed, cleaning up..." << std::endl;

        eventloop.remove_event_handler(this);

        if (slave_client)
        {
            close(slave_client->fd);
            slave_client->fd = -1;
        }

        connected = false;

        // Notify server about disconnection
        if (disconnect_callback)
        {
            disconnect_callback();
        }
    }

    void MasterConnection::send(std::string &data)
    {
        if (!slave_client || data.empty())
            return;

        slave_client->write_buffer.append(data);

        eventloop.update_event_handler(this, READ_EVENT | WRITE_EVENT);
    }

    bool MasterConnection::connect()
    {
        return connected;
    }
}
