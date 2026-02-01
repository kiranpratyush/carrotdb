#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <chrono>
#include "parser/command.h"

#ifndef SERVER_NAMESPACE
#define SERVER_NAMESPACE server
#endif

using namespace REDIS_NAMESPACE;

// Forward declaration
namespace CONNECTION_NAMEAPACE
{
    class TcpConnection;
}

namespace SERVER_NAMESPACE
{
    struct ClientStates
    {
        const static uint64_t MULTI = (1 << 0);
        const static uint64_t DIRTY = (1 << 1);
    };
    struct Client
    {
        std::string read_buffer{};
        std::string write_buffer{};
        uint64_t flags{};
        std::vector<std::unique_ptr<Command>> queued_commands{};
        int fd{};
        bool isslave{};
        int64_t replica_offset{0}; // Track replica's acknowledged offset (for slaves)

        Client() = default;

        explicit Client(unsigned int initial_capacity)
        {
            read_buffer.reserve(initial_capacity);
            write_buffer.reserve(initial_capacity);
        }
        void resetFlags()
        {
            flags = 0;
        }
        void addTransactionFlags()
        {
            flags |= (ClientStates::MULTI);
        }
        bool isClientOnTransaction()
        {
            return flags && (ClientStates::MULTI);
        }
        void setClientDirty()
        {
            flags |= (ClientStates::DIRTY);
        }
    };
    struct ClientContext
    {
        std::shared_ptr<Client> client{};
        int client_fd;
        unsigned long long current_read_position{};
        unsigned long long current_write_position{};
        bool isBlocked{false};
        int unblocked_client_fd{-1};
        CONNECTION_NAMEAPACE::TcpConnection *connection{nullptr};
        std::unique_ptr<Command> command{nullptr}; // Parsed command (parse once, use everywhere)
        size_t command_bytes{0};                   // Bytes consumed from read_buffer for this command

        ClientContext(std::shared_ptr<Client> client, int fd, CONNECTION_NAMEAPACE::TcpConnection *conn = nullptr)
            : client{client}, client_fd{fd}, connection{conn}
        {
        }
    };
    struct BlockedClient
    {
        std::weak_ptr<Client> client;
        int client_fd;
        std::chrono::steady_clock::time_point timeout_at;
        double timeout_seconds;

        BlockedClient(std::weak_ptr<Client> c, int fd, double timeout_secs)
            : client(c), client_fd(fd), timeout_seconds(timeout_secs)
        {
            if (timeout_secs == 0.0)
            {
                // 0 means block indefinitely - set to max time point
                timeout_at = std::chrono::steady_clock::time_point::max();
            }
            else
            {
                // Convert seconds (with fractional part) to milliseconds
                auto timeout_ms = std::chrono::milliseconds(static_cast<long long>(timeout_secs * 1000));
                timeout_at = std::chrono::steady_clock::now() + timeout_ms;
            }
        }

        bool is_expired() const
        {
            return timeout_at != std::chrono::steady_clock::time_point::max() &&
                   std::chrono::steady_clock::now() >= timeout_at;
        }
    };
    struct BlockedXreadClient
    {
        std::weak_ptr<Client> client;
        int client_fd;
        std::chrono::steady_clock::time_point timeout_at;
        std::string stream_id;
        std::string stream_key;
        double timeout_seconds;
        BlockedXreadClient(std::weak_ptr<Client> c, int fd, double timeout_ms) : client(c), client_fd(fd), timeout_seconds(timeout_ms / 1000)
        {
            if (timeout_ms == 0)
            {
                // 0 means block indefinitely - set to max time point
                timeout_at = std::chrono::steady_clock::time_point::max();
            }
            else
            {
                // Convert seconds (with fractional part) to milliseconds
                auto timeoutMs = std::chrono::milliseconds(static_cast<long long>(timeout_ms));
                timeout_at = std::chrono::steady_clock::now() + timeoutMs;
            }
        }
        bool is_expired() const
        {
            return timeout_at != std::chrono::steady_clock::time_point::max() && std::chrono::steady_clock::now() >= timeout_at;
        }
    };
    struct OngoingTransactionClient
    {
        std::weak_ptr<Client> client;
        int client_fd;
        OngoingTransactionClient(std::weak_ptr<Client> c, int fd) : client(c), client_fd(fd) {}
    };

    struct BlockedWaitClient
    {
        std::weak_ptr<Client> client;
        int client_fd;
        std::chrono::steady_clock::time_point timeout_at;
        uint64_t num_replicas_needed;
        int64_t master_offset;

        BlockedWaitClient(std::weak_ptr<Client> c, int fd, uint64_t timeout_ms, uint64_t num_replicas, int64_t offset)
            : client(c), client_fd(fd), num_replicas_needed(num_replicas), master_offset(offset)
        {
            if (timeout_ms == 0)
            {
                timeout_at = std::chrono::steady_clock::time_point::max();
            }
            else
            {
                auto timeoutMs = std::chrono::milliseconds(static_cast<long long>(timeout_ms));
                timeout_at = std::chrono::steady_clock::now() + timeoutMs;
            }
        }

        bool is_expired() const
        {
            return timeout_at != std::chrono::steady_clock::time_point::max() &&
                   std::chrono::steady_clock::now() >= timeout_at;
        }
    };
}
