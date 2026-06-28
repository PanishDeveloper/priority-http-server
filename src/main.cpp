#include "server.hpp"

int main(int argc, char* argv[])
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
