#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <functional>

#include "logger.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

class HttpServer
{
public:
    explicit HttpServer(boost::asio::io_context& ioc, unsigned short port,
                        size_t                   numThreads = std::thread::hardware_concurrency(),
                        std::unique_ptr<LogSink> sink       = std::make_unique<ConsoleSink>());
    void run();

private:
    void setup();
    void startAcceptorLoop();
    void shutdown();
    void doAccept();
    void handleSession(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                       std::function<void()>                                restartAccept);

    boost::beast::http::response<boost::beast::http::string_body> processRequest(
        const boost::beast::http::request<boost::beast::http::string_body>& req) const;

    void asyncSendResponse(
        std::shared_ptr<boost::asio::ip::tcp::socket>                                  socket,
        std::shared_ptr<boost::beast::http::response<boost::beast::http::string_body>> response,
        std::function<void()> restartAccept);

    boost::asio::io_context&                        m_ioc;
    unsigned short                                  m_port;
    ThreadPool                                      m_pool;
    Router                                          m_router;
    AsyncLogger                                     m_logger;
    boost::asio::signal_set                         m_signals;
    std::vector<std::thread>                        m_ioThreads;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
};
