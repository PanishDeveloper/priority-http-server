#include "server.hpp"

#include <iostream>

int main()
{
    std::cout << "Server starting..." << std::endl;
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    HttpServer server(8080);
    server.run();

    return 0;
}
