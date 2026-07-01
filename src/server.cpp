#include "server.hpp"

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/system/errc.hpp>
#include <memory>

#include "session.hpp"
#include "static_file_handler.hpp"
#include "status_handler.hpp"
#include "utils.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = asio::ip::tcp;

HttpServer::HttpServer(asio::io_context& ioc, const Config& config)
    : m_ioc(ioc),
      m_config(config),
      m_pool(config.threads, config.max_queue_size),
      m_logger(config.log_file_path.empty()
                   ? std::unique_ptr<LogSink>(std::make_unique<ConsoleSink>())
                   : std::unique_ptr<LogSink>(std::make_unique<FileSink>(config.log_file_path))),
      m_signals(m_ioc, SIGINT, SIGTERM),
      m_drainTimer(m_ioc)
{
    m_processor = std::make_unique<RequestProcessor>(m_router, *this, m_pool, m_ioc);
    setLogLevel(m_config.log_level);
}

HttpServer::~HttpServer()
{
    shutdown();
}

void HttpServer::run()
{
    setup();
    startAcceptorLoop();
    m_ioc.run();
    shutdown();
}

void HttpServer::setup()
{
    m_logger.start();
    m_logger.log("Server starting... Compute: " + std::to_string(m_config.threads) +
                     " threads, I/O: " + std::to_string(m_config.io_threads) + " threads",
                 LogLevel::INFO);

    m_router.addRoute(http::verb::get, "/status", std::make_unique<StatusHandler>(m_pool));
    m_router.addRoute(http::verb::get, "/static/",
                      std::make_unique<StaticFileHandler>(
                          m_config.static_root, m_config.static_max_file_size_mb * 1024 * 1024));
    m_router.addRoute(http::verb::head, "/static/",
                      std::make_unique<StaticFileHandler>(
                          m_config.static_root, m_config.static_max_file_size_mb * 1024 * 1024));

    m_pool.start();

    m_signals.async_wait(
        [this](const boost::system::error_code& ec, int signal)
        {
            if (!ec)
            {
                m_logger.log("Shutdown signal received (SIG " + std::to_string(signal) + ")",
                             LogLevel::INFO);
                m_draining = true;

                m_drainTimer.expires_after(m_config.drain_timeout_sec);
                m_drainTimer.async_wait(
                    [this](const boost::system::error_code& ec)
                    {
                        if (ec == asio::error::operation_aborted)
                            return;

                        m_logger.log("Drain timeout reached, forcing remaining sessions closed",
                                     LogLevel::WARNING);

                        std::vector<std::shared_ptr<Session>> remaining;
                        {
                            std::unique_lock lock(m_sessionsMutex);
                            remaining.assign(m_sessions.begin(), m_sessions.end());
                        }
                        for (auto& s : remaining) s->close();
                        std::unique_lock lock(m_sessionsMutex);
                        checkDrainComplete();
                    });
                std::unique_lock lock(m_sessionsMutex);
                checkDrainComplete();
            }
        });
}

void HttpServer::startAcceptorLoop()
{
    asio::ip::address address = asio::ip::make_address(m_config.bind_address);
    tcp::endpoint     endpoint(address, m_config.port);
    m_acceptor = std::make_unique<tcp::acceptor>(m_ioc, endpoint);
    m_logger.log(
        "Server listening on " + m_config.bind_address + ":" + std::to_string(m_config.port),
        LogLevel::INFO);

    doAccept();

    unsigned int numIoThreads = m_config.io_threads;
    if (numIoThreads == 0)
        numIoThreads = 1;
    m_ioThreads.reserve(numIoThreads);
    for (unsigned int i = 0; i < numIoThreads; ++i)
        m_ioThreads.emplace_back([this] { m_ioc.run(); });
}

void HttpServer::doAccept()
{
    if (m_ioc.stopped() || m_draining)
        return;

    // Checking the connection limit
    if (m_activeSessions.load() >= m_config.max_connections)
    {
        m_logger.log("Max connections reached (" + std::to_string(m_config.max_connections) +
                         "), rejecting new connection",
                     LogLevel::WARNING);
        // Accept socket and close immediately so client gets a reset
        auto socket = std::make_shared<tcp::socket>(m_ioc);
        m_acceptor->async_accept(*socket,
                                 [this, socket](const boost::system::error_code& ec)
                                 {
                                     if (!ec)
                                     {
                                         boost::system::error_code close_ec;
                                         std::ignore = socket->close(close_ec);
                                     }
                                     doAccept();
                                 });
        return;
    }

    auto socket = std::make_shared<tcp::socket>(m_ioc);

    m_acceptor->async_accept(
        *socket,
        [this, socket](const boost::system::error_code& ec)
        {
            if (ec)
            {
                if (ec == asio::error::operation_aborted)
                {
                    m_logger.log("Accept interrupted by shutdown, exiting...", LogLevel::INFO);
                    return;
                }
                m_logger.log(std::string("Accept error: ") + ec.message(), LogLevel::ERROR);
                doAccept();
                return;
            }
            doAccept();

            auto session = std::make_shared<Session>(std::move(*socket), m_ioc, *this, *m_processor,
                                                     m_logger, m_config.keepalive_timeout_sec,
                                                     m_config.max_keepalive_requests,
                                                     m_config.enable_keepalive);
            {
                std::unique_lock lock(m_sessionsMutex);
                m_sessions.insert(session);
                ++m_activeSessions;
            }
            session->start();
        });
}

// Asynchronous sending of a response
void HttpServer::sendResponse(
    const std::shared_ptr<Session>&                                session,
    std::shared_ptr<http::response<http::string_body>>             response,
    const std::shared_ptr<const http::request<http::string_body>>& request) const
{
    response->set(http::field::server, m_config.server_name);
    if (!m_config.cors_allow_origin.empty())
    {
        response->set(http::field::access_control_allow_origin, m_config.cors_allow_origin);
    }

    bool keepAlive = isKeepAlive(request) && m_config.enable_keepalive;
    session->sendResponse(std::move(response), keepAlive);
}

void HttpServer::endSession(const std::shared_ptr<Session>& session)
{
    size_t activeAfter = 0;
    // Deleting it from the storage
    {
        std::unique_lock lock(m_sessionsMutex);
        m_sessions.erase(session);
        --m_activeSessions;
        activeAfter = m_activeSessions.load();
        if (m_draining)
            checkDrainComplete();
    }
    m_logger.log("Session ended. Active: " + std::to_string(activeAfter), LogLevel::DEBUG);
}

void HttpServer::checkDrainComplete()
{
    if (m_draining && m_activeSessions.load() == 0)
    {
        m_drainTimer.cancel();
        m_logger.log("All sessions finished, stopping I/O", LogLevel::INFO);
        m_ioc.stop();
    }
}

// Determines whether to keep the connection alive after sending a response
bool HttpServer::isKeepAlive(
    const std::shared_ptr<const http::request<http::string_body>>& request) noexcept
{
    // If there is a write error or is no request, close the connection
    if (!request)
        return false;

    const auto& req = *request;
    auto        it  = req.find(http::field::connection);

    // If the Connection header is absent, keep-alive by default (HTTP/1.1)
    if (it == req.end())
        return true;

    std::string connectionValue = std::string(it->value());
    boost::algorithm::to_lower(connectionValue);

    // If Connection: close, close the connection; otherwise keep-alive
    return connectionValue != "close";
}

void HttpServer::shutdown()
{
    bool expected = false;
    if (!m_shutdownDone.compare_exchange_strong(expected, true))
        return;

    m_draining = true;
    {
        std::vector<std::shared_ptr<Session>> sessionsToClose;
        {
            std::unique_lock lock(m_sessionsMutex);
            sessionsToClose.assign(m_sessions.begin(), m_sessions.end());
        }
        for (auto& session : sessionsToClose) session->close();
    }

    for (auto& th : m_ioThreads)
        if (th.joinable())
            th.join();

    m_logger.log("Shutting down pool...", LogLevel::INFO);
    size_t abandonedWorkers = m_pool.shutdownWithTimeout(m_config.drain_timeout_sec);
    if (abandonedWorkers > 0)
    {
        m_logger.log("Warning: " + std::to_string(abandonedWorkers) +
                         " worker thread(s) did not finish within drain_timeout_sec and were "
                         "abandoned (detached) — server is exiting anyway",
                     LogLevel::WARNING);
    }

    m_logger.log("Shutting down logger...", LogLevel::INFO);
    m_logger.log("Server stopped.", LogLevel::INFO);
    m_logger.shutdown();
}