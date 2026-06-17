#pragma once

#include <boost/asio/io_context.hpp>
#include "thread_pool.hpp"
#include "router.hpp"

class HttpServer
{
public:
    explicit HttpServer(unsigned short port, size_t numThreads = std::thread::hardware_concurrency());
    void run();

private:
    unsigned short m_port;
    boost::asio::io_context m_ioc;
    ThreadPool m_pool;
    Router m_router;
};
