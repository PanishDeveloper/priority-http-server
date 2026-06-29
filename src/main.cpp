#include <iostream>

#include "server.hpp"

int main(int argc, char* argv[])
{
    try
    {
        if (hasHelpFLag(argc, argv))
        {
            printHelp();
            return 0;
        }

        auto config = loadConfig(argc, argv);

        boost::asio::io_context ioc;
        HttpServer              server(ioc, config);
        server.setLogLevel(config.log_level);
        server.run();

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FATAL] Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "[FATAL] Unknown exception" << std::endl;
        return 1;
    }
}
