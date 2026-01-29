#include "server.h"
#include "networking.h"
#include "replication.h"
#include "utils/utils.h"

namespace SERVER_NAMESPACE
{
    using namespace NETWORKING;

    int Server::make_nonblocking(int &fd)
    {
        // get the flags set on the fd
        int status = fcntl(fd, F_GETFL);
        if (status == -1)
        {
            return -1;
        }
        status |= O_NONBLOCK;
        int new_status = fcntl(fd, F_SETFL, status);
        return new_status;
    }

    int Server::setup()
    {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        // make server_fd as non blocking
        if (make_nonblocking(server_fd) == -1)
        {
            std::cerr << errno << "\n";
            return 1;
        };
        if (server_fd < 0)
        {
            std::cerr << "Failed to create server socket\n";
            return 1;
        }
        int reuse = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        {
            std::cerr << "setsockopt failed\n";
            return 1;
        }

        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
        {
            std::cerr << "Failed to bind to port 6379\n";
            return 1;
        }
        if (listen(server_fd, connection_backlog) != 0)
        {
            std::cerr << "Listen failed\n";
            return 1;
        }
        if (config.role == ServerRole::SLAVE)
        {
            int sockfd{};
            auto is_success = connect_client(config.master_host, config.master_port, sockfd);
            if (is_success)
            {
                // Make master socket non-blocking for edge-triggered epoll
                make_nonblocking(sockfd);
                master_client = std::make_unique<REPLICATION_NAMESPACE::MasterClient>(config.port);
                master_client->fd = sockfd;
                master_client->set_db_and_config(&db, &config);
            }
        }
        std::cout
            << "Waiting for a client to connect...\n";
        return 0;
    }
    void Server::run(std::string &buffer, int buffer_size)
    { // create an epoll instance , can we move this to the constructor
        epoll_instance_fd = epoll_create1(0);
        if (epoll_instance_fd == -1)
        {
            perror("epoll_create1");
            return;
        }
        // register the server_fd for EPOLL_IN event
        epoll_event event{};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = server_fd;
        if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
        {
            perror("epoll_ctl");
            return;
        }
        if (config.role == ServerRole::SLAVE && master_client)
        {
            bool status = register_to_eventloop_for_write(master_client->fd, true);
            if (!status)
            {
                perror("Failed to create master client on slave mode");
                return;
            }
        }
        epoll_event events[10];
        while (true)
        {
            // Get the timeout for the next blocked client to expire
            int timeout_ms = db.get_next_timeout_ms();
            int n = epoll_wait(epoll_instance_fd, events, 10, timeout_ms);
            if (n == -1)
            {
                perror("epoll_wait");
                break;
            }

            // Check for expired blocked clients (timeout occurred or after processing events)
            handle_expired_blocked_clients();
            for (int i = 0; i < n; i++)
            {
                // If the event is from server_fd continously accept the client request
                if (events[i].data.fd == server_fd)
                {
                    handle_new_client_connection();
                }
                else if (config.role == ServerRole::SLAVE && events[i].data.fd == master_client->fd)
                {
                    bool should_invert = false;
                    bool is_read_event = events[i].events & EPOLLIN;
                    bool is_write_event = events[i].events & EPOLLOUT;
                    std::cout << "[SLAVE] Event on master_fd: read=" << is_read_event
                              << " write=" << is_write_event << std::endl;
                    if (events[i].events & (EPOLLERR | EPOLLHUP))
                    {
                        std::cout << "[SLAVE] Error or hangup on master connection" << std::endl;
                        close(events[i].data.fd);
                    }
                    master_client->handle_master(is_read_event, should_invert);
                    std::cout << "[SLAVE] After handle_master: should_invert=" << should_invert << std::endl;
                    if (should_invert)
                    {
                        if (is_read_event)
                        {
                            std::cout << "[SLAVE] Switching to WRITE mode" << std::endl;
                            register_to_eventloop_for_write(master_client->fd, false);
                        }
                        if (is_write_event)
                        {
                            std::cout << "[SLAVE] Switching to READ mode" << std::endl;
                            register_to_eventloop_for_read(master_client->fd, false);
                        }
                    }
                    else
                    {
                        // Even if not inverting, we need to re-register for read mode
                        // to ensure edge-triggered epoll continues to work after handshake
                        std::cout << "[SLAVE] Re-registering for READ mode (staying in read)" << std::endl;
                        register_to_eventloop_for_read(master_client->fd, false);
                    }
                }
                else
                {
                    // These are client fd
                    if (events[i].events & EPOLLIN)
                    {
                        handle_read(events[i].data.fd);
                    }
                    if (events[i].events & EPOLLOUT)
                    {
                        handle_write(events[i].data.fd);
                    }
                    if (events[i].events & (EPOLLERR | EPOLLHUP))
                    {
                        close(events[i].data.fd);
                    }
                }
            }
        }
        close(epoll_instance_fd);
        close(server_fd);
    }

    int Server::handle_read(int fd)
    {
        auto client = active_clients[fd];
        auto client_context = ClientContext{client, fd};
        char temp[300];
        while (true)
        {
            int size = read(fd, temp, 300);
            if (size == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    bool status = replicationManager.handle(client_context, config);
                    if (!status)
                    {
                        db.execute(client_context, &config);
                        notifyslaves(client_context);
                    }

                    if (!client_context.isBlocked)
                    {
                        epoll_event event{};
                        event.events = EPOLLOUT | EPOLLET;
                        event.data.fd = fd;
                        if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_MOD, fd, &event) == -1)
                        {
                            perror("epoll_ctl MOD for write");
                            return -1;
                        }
                    }
                    if (client_context.unblocked_client_fd != -1)
                    {
                        epoll_event event{};
                        event.events = EPOLLOUT | EPOLLET;
                        event.data.fd = client_context.unblocked_client_fd;
                        if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_MOD, client_context.unblocked_client_fd, &event) == -1)
                        {
                            perror("epoll_ctl MOD for write (unblocked client)");
                            return -1;
                        }
                    }

                    return 0; // Successfully read all available data
                }
                else
                {
                    perror("read");
                    epoll_ctl(epoll_instance_fd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    return -1;
                }
            }

            if (size > 0)
            {
                std::cout << "Read " << size << " bytes from client" << std::endl;
                client->read_buffer.append(temp, size);
                continue;
            }

            if (size == 0)
            {
                std::cout << "Client closed connection" << std::endl;
                epoll_ctl(epoll_instance_fd, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
                return -1;
            }
        }
    }

    int Server::handle_write(int fd)
    {
        auto client = active_clients[fd];
        std::cout << "[handle_write] fd=" << fd << " isslave=" << client->isslave
                  << " buffer_size=" << client->write_buffer.size() << std::endl;
        size_t total_size = client->write_buffer.size();
        size_t written = 0;

        while (written < total_size)
        {
            int size = write(fd, client->write_buffer.data() + written, total_size - written);
            if (size == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // Socket buffer is full, we'll be notified again when we can write
                    std::cout << "Write would block, written " << written << " of " << total_size << " bytes" << std::endl;
                    return 0;
                }
                else
                {
                    // Real error occurred
                    perror("write");
                    epoll_ctl(epoll_instance_fd, EPOLL_CTL_DEL, fd, nullptr);
                    active_clients.erase(fd);
                    close(fd);
                    return -1;
                }
            }

            if (size > 0)
            {
                written += size;
                std::cout << "Wrote " << size << " bytes, total: " << written << "/" << total_size << std::endl;
            }
        }
        epoll_event event{};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = fd;
        /*Clear the buffer after writing*/
        client->write_buffer.clear();
        // Only clear read_buffer for non-slave clients
        if (!client->isslave)
        {
            client->read_buffer.clear();
        }
        if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_MOD, fd, &event) == -1)
        {
            perror("epoll_ctl MOD back to read");
            return -1;
        }

        if (client->isslave)
        {
            std::cout << "Slave " << fd << " back to read-only mode (waiting for ACKs)" << std::endl;
        }
        else
        {
            std::cout << "Response sent successfully, back to read mode" << std::endl;
        }
        return 0;
    }

    int Server::handle_new_client_connection()
    {
        while (true)
        {
            int client_fd = accept(server_fd, (sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
            if (client_fd == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    return 0;
                }
                else
                {
                    perror("accept");
                    return -1;
                }
            }

            make_nonblocking(client_fd);
            auto client = std::make_shared<Client>(MAX_BUFFER_SIZE);
            client->fd = client_fd;
            active_clients.emplace(client_fd, client);
            epoll_event client_event{};
            client_event.events = EPOLLIN | EPOLLET;
            client_event.data.fd = client_fd;
            if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1)
            {
                perror("epoll_ctl client");
                close(client_fd);
                return -1;
            }
        }
    }

    void Server::handle_expired_blocked_clients()
    {
        std::vector<int> expired_fds = db.check_and_expire_blocked_clients();

        for (int fd : expired_fds)
        {
            // Make the expired client ready for writing (to send timeout response)
            epoll_event event{};
            event.events = EPOLLOUT | EPOLLET;
            event.data.fd = fd;
            if (epoll_ctl(epoll_instance_fd, EPOLL_CTL_MOD, fd, &event) == -1)
            {
                perror("epoll_ctl MOD for expired client");
            }
        }
    }

    void Server::notifyslaves(ClientContext client_context)
    {
        std::cout << "[MASTER] notifyslaves called" << std::endl;
        bool propagated = replicationManager.handle_propagate(client_context);
        std::cout << "[MASTER] handle_propagate returned: " << propagated << std::endl;
        replicationManager.for_each_active_slaves([this](std::shared_ptr<Client> slave)
                                                  {
        std::cout << "[MASTER] Checking slave fd=" << slave->fd 
                  << " write_buffer size=" << slave->write_buffer.size() << std::endl;
        if (!slave->write_buffer.empty()) {
            std::cout << "[MASTER] Slave " << slave->fd << " is scheduled for write: " 
                      << slave->write_buffer.size() << " bytes" << std::endl;
            epoll_event event{};
            event.events = EPOLLOUT | EPOLLET;
            event.data.fd = slave->fd;
            int result = epoll_ctl(epoll_instance_fd, EPOLL_CTL_MOD, slave->fd, &event);
            if (result == -1) {
                std::cout << "[MASTER] epoll_ctl MOD FAILED, errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
            } else {
                std::cout << "[MASTER] epoll_ctl MOD success" << std::endl;
            }
        } });
    }

    bool Server::register_to_eventloop_for_read(int fd, bool is_first_time)
    {
        epoll_event event{};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = fd;
        auto op_type = is_first_time ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
        auto status = epoll_ctl(epoll_instance_fd, op_type, fd, &event);
        return status == 0;
    }
    bool Server::register_to_eventloop_for_write(int fd, bool is_first_time)
    {
        epoll_event event{};
        event.events = EPOLLOUT | EPOLLET;
        event.data.fd = fd;
        auto op_type = is_first_time ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
        auto status = epoll_ctl(epoll_instance_fd, op_type, fd, &event);
        return status == 0;
    }
}