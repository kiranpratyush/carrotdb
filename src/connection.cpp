#include "connection.h"
#include "eventloop.h"
#include <fcntl.h>
#include <iostream>

using namespace EVENT_LOOP_NAMESPACE;

namespace CONNECTION_NAMEAPACE
{
    TcpConnection::TcpConnection(std::shared_ptr<Client> _client, IEventLoop &_eventLoop, onMessageCallBack _callback)
        : client(_client), eventloop(_eventLoop), cb(_callback)
    {
        // Ensure socket is non-blocking for edge-triggered epoll
        int flags = fcntl(client->fd, F_GETFL, 0);
        if (flags != -1)
        {
            if (fcntl(client->fd, F_SETFL, flags | O_NONBLOCK) == -1)
            {
                std::cerr << "Warning: Failed to set socket non-blocking\n";
            }
        }
    }

    void TcpConnection::handleRead()
    {
        char temp[1024];
        while (true)
        {
            ssize_t n = read(client->fd, temp, sizeof(temp));
            if (n > 0)
            {
                client->read_buffer.append(temp, n);
            }
            else if (n == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    if (cb)
                        cb(this);
                    break;
                }
                handleError();
                return;
            }
            else
            {
                handleClose();
                return;
            }
        }
    }

    void TcpConnection::handleWrite()
    {
        if (client->write_buffer.empty())
        {
            eventloop.update_event_handler(this, READ_EVENT);
        }

        size_t written_total = 0;
        while (written_total < client->write_buffer.size())
        {
            ssize_t n = write(client->fd, client->write_buffer.data() + written_total, client->write_buffer.size() - written_total);
            if (n > 0)
            {
                written_total += n;
            }
            else if (n == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    client->write_buffer.erase(0, written_total);
                    return;
                }
                handleError();
                return;
            }
        }
        client->write_buffer.clear();
        eventloop.update_event_handler(this, READ_EVENT);
    }

    void TcpConnection::send(std::string &data)
    {
        if (data.empty())
            return;
        bool was_empty = client->write_buffer.empty();

        client->write_buffer.append(data);
        if (was_empty)
            eventloop.update_event_handler(this, READ_EVENT | WRITE_EVENT);
    }

    void TcpConnection::flush()
    {
        if (!client->write_buffer.empty())
        {
            eventloop.update_event_handler(this, READ_EVENT | WRITE_EVENT);
        }
    }

    void TcpConnection::handleClose()
    {
        eventloop.remove_event_handler(this);
    }
    void TcpConnection::handleError()
    {
        eventloop.remove_event_handler(this);
    }
}