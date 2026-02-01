#include "server.h"
#include "networking.h"
#include "replication.h"
#include "utils/utils.h"
#include "parser/command-parser.h"

using namespace EVENT_LOOP_NAMESPACE;
using namespace CONNECTION_NAMEAPACE;
using namespace ACCEPTOR_NAMESPACE;
using namespace MASTER_CONNECTION_NAMESPACE;

namespace SERVER_NAMESPACE
{
    using namespace NETWORKING;

    int Server::setup()
    {
        // Create event loop
        eventloop = std::make_unique<EpollEventLoop>();

        auto callback = [this](std::shared_ptr<Client> client)
        {
            onNewConnection(client);
        };
        acceptor = std::make_unique<Acceptor>("127.0.0.1", config.port, *eventloop, callback);

        if (!acceptor->isValid())
        {
            std::cerr << "Failed to create acceptor\n";
            return 1;
        }

        acceptor->start();

        if (config.role == ServerRole::SLAVE)
        {
            master_connection = std::make_unique<MasterConnection>(
                config.master_host,
                config.master_port,
                config.port,
                *eventloop,
                &db,
                &config);

            if (!master_connection->isConnected())
            {
                std::cerr << "Failed to connect to master\n";
                return 1;
            }
            master_connection->setDisconnectCallback([this]()
                                                     { handle_master_disconnection(); });
        }

        std::cout << "Waiting for a client to connect...\n";
        return 0;
    }
    void Server::run(std::string &buffer, int buffer_size)
    {
        std::cout << "Server running with reactor pattern...\n";

        while (true)
        {
            int timeout_ms = db.get_next_timeout_ms();
            int wait_timeout_ms = replicationManager.get_next_wait_timeout_ms();
            if (wait_timeout_ms >= 0)
            {
                if (timeout_ms < 0)
                    timeout_ms = wait_timeout_ms;
                else
                    timeout_ms = std::min(timeout_ms, wait_timeout_ms);
            }

            eventloop->run(timeout_ms);

            handle_expired_blocked_clients();
        }
    }

    void Server::onNewConnection(std::shared_ptr<Client> client)
    {
        auto message_callback = [this](TcpConnection *conn)
        {
            onMessage(conn);
        };

        auto connection = std::make_shared<TcpConnection>(client, *eventloop, message_callback);
        active_connections[client->fd] = connection;

        eventloop->register_event_handler(connection.get(), READ_EVENT);
    }
    void Server::onMessage(TcpConnection *conn)
    {
        processClientRequest(conn);
    }
    void Server::processClientRequest(TcpConnection *conn)
    {
        int fd = conn->getfd();
        auto client = active_connections[fd];
        if (!client)
        {
            std::cerr << "Connection not found for fd=" << fd << std::endl;
            return;
        }

        auto client_context = ClientContext{client->getClient(), fd, client.get()};

        client_context.command = CommandParser::parseCommand(client_context);
        if (!client_context.command)
        {
            std::cerr << "Failed to parse command for fd=" << fd << std::endl;
            return;
        }
        bool status = replicationManager.handle(client_context, config);
        if (!status)
        {
            db.execute(client_context, &config);
            replicationManager.propagateAndNotifySlaves(client_context, config);
        }

        if (!client_context.isBlocked)
        {
            client_context.connection->flush();
        }

        if (client_context.unblocked_client_fd != -1)
        {
            auto unblocked_conn = active_connections[client_context.unblocked_client_fd];
            if (unblocked_conn)
            {
                unblocked_conn->flush();
            }
        }
    }

    void Server::handle_expired_blocked_clients()
    {
        std::vector<int> expired_fds = db.check_and_expire_blocked_clients();

        for (int fd : expired_fds)
        {
            auto conn = active_connections[fd];
            if (conn)
            {
                conn->flush();
            }
        }

        std::vector<int> expired_wait_fds = replicationManager.check_and_expire_blocked_wait_clients();

        for (int fd : expired_wait_fds)
        {
            auto conn = active_connections[fd];
            if (conn)
            {
                conn->flush();
            }
        }
    }

    void Server::handle_master_disconnection()
    {
        std::cerr << "[SLAVE] Lost connection to master. Shutting down..." << std::endl;
        master_connection.reset();
        std::exit(1);
    }
}