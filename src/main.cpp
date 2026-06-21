#include "server.hpp"

int main()
{
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    boost::asio::io_context ioc;
    HttpServer server(ioc, 8080, numThreads, std::make_unique<ConsoleSink>());
    server.run();

    return 0;
}
