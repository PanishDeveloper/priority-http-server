#include "server.hpp"

int main()
{
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    HttpServer server(8080, numThreads, std::make_unique<ConsoleSink>());
    server.run();

    return 0;
}
