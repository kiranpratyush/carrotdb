#include "server.h"
#include <cstring>
#include <cstdlib>

namespace SERVER_NAMESPACE
{
    void Server::initialize(int argc, char **argv)
    {
        // Check if argc > 1 and (argc - 1) is even
        if (argc > 1)
        {
            if ((argc - 1) % 2 != 0)
            {
                std::cerr << "Error: Invalid number of arguments. Flags require values." << std::endl;
                return;
            }
        }

        for (int i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], "--port") == 0)
            {
                if (i + 1 < argc)
                {
                    int port = atoi(argv[i + 1]);
                    if (port > 0 && port <= 65535)
                    {
                        config.port = port;
                        server_addr.sin_port = htons(port);
                        std::cout << "Using port: " << port << std::endl;
                    }
                    else
                    {
                        std::cerr << "Invalid port number: " << argv[i + 1] << std::endl;
                    }
                    i++;
                }
                else
                {
                    std::cerr << "--port flag requires a port number" << std::endl;
                }
            }
            if (is_equal(argv[i], "--replicaof"))
            {
                if (i + 1 < argc)
                {
                    std::string replica_arg = argv[i + 1];
                    size_t space_pos = replica_arg.find(' ');

                    if (space_pos != std::string::npos)
                    {
                        config.master_host = replica_arg.substr(0, space_pos);
                        config.master_port = atoi(replica_arg.substr(space_pos + 1).c_str());

                        if (config.master_port > 0 && config.master_port <= 65535)
                        {
                            config.role = ServerRole::SLAVE;
                            std::cout << "Configured as slave of " << config.master_host
                                      << ":" << config.master_port << std::endl;
                        }
                        else
                        {
                            std::cerr << "Invalid master port number in: " << replica_arg << std::endl;
                        }
                        i++;
                    }
                    else
                    {
                        std::cerr << "--replicaof flag requires hostname and port" << std::endl;
                    }
                }
                else
                {
                    std::cerr << "--replicaof flag requires hostname and port" << std::endl;
                }
            }
        }
    }

}