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
                // Check if port value is provided
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
                    i++; // Skip the next argument since we've consumed it
                }
                else
                {
                    std::cerr << "--port flag requires a port number" << std::endl;
                }
            }
        }
    }

}