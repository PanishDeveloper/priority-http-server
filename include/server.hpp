#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <functional>

#include "logger.hpp"
#include "request_processor.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

namespace http = boost::beast::http;

class HttpServer
{
public:
    explicit HttpServer(boost::asio::io_context& ioc, unsigned short port,
                        size_t                   numThreads = std::thread::hardware_concurrency(),
                        std::unique_ptr<LogSink> sink       = std::make_unique<ConsoleSink>());
    void run();
    void setLogLevel(LogLevel level) { m_logger.setMinLevel(level); }
    void sendResponse(
        std::shared_ptr<boost::asio::ip::tcp::socket>                                 socket,
        std::shared_ptr<http::response<http::string_body>>                            response,
        std::optional<std::reference_wrapper<const http::request<http::string_body>>> request,
        std::function<void()> restartAccept);

private:
    void setup();
    void startAcceptorLoop();
    void shutdown();
    void doAccept();
    void handleSession(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                       std::function<void()>                                restartAccept);

    static bool isKeepAlive(
        const std::optional<std::reference_wrapper<const http::request<http::string_body>>>&
                                         request,
        const boost::system::error_code& ec);

    boost::asio::io_context&                        m_ioc;
    unsigned short                                  m_port;
    ThreadPool                                      m_pool;
    Router                                          m_router;
    AsyncLogger                                     m_logger;
    boost::asio::signal_set                         m_signals;
    std::vector<std::thread>                        m_ioThreads;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
    std::unique_ptr<RequestProcessor>               m_processor;
};
