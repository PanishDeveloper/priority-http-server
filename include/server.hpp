#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include "logger.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

class HttpServer
{
public:
    explicit HttpServer (boost::asio::io_context& ioc, unsigned short port,
                         size_t                   numThreads = std::thread::hardware_concurrency (),
                         std::unique_ptr<LogSink> sink       = std::make_unique<ConsoleSink> ());
    void run ();

private:
    boost::asio::io_context& m_ioc;
    unsigned short           m_port;
    ThreadPool               m_pool;
    Router                   m_router;
    AsyncLogger              m_logger;
    boost::asio::signal_set  m_signals;
    bool                     m_shutdownRequested = false;
};
