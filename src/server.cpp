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

HttpServer::HttpServer(asio::io_context& ioc, unsigned short port, size_t numThreads,
                       std::unique_ptr<LogSink> sink)
    : m_ioc(ioc),
      m_port(port),
      m_pool(numThreads),
      m_logger(std::move(sink)),
      m_signals(m_ioc, SIGINT, SIGTERM)
{
    m_processor = std::make_unique<RequestProcessor>(m_router, *this, m_pool, m_ioc);
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
    m_logger.log("Server starting...", LogLevel::INFO);

    m_router.addRoute(http::verb::get, "/status", std::make_unique<StatusHandler>(m_pool));
    m_router.addRoute(http::verb::get, "/static/",
                      std::make_unique<StaticFileHandler>("static", 10 * 1024 * 1024));
    m_router.addRoute(http::verb::head, "/static/",
                      std::make_unique<StaticFileHandler>("static", 10 * 1024 * 1024));

    m_pool.start();

    m_signals.async_wait(
        [this](const boost::system::error_code& ec, int signal)
        {
            if (!ec)
            {
                m_logger.log("Shutdown signal received (SIG " + std::to_string(signal) + ")",
                             LogLevel::INFO);
                m_draining = true;

                // When the signal is given, we close all sessions
                std::vector<std::shared_ptr<Session>> sessionsToClose;
                {
                    std::unique_lock lock(m_sessionsMutex);
                    sessionsToClose.assign(m_sessions.begin(), m_sessions.end());
                }
                for (auto& session : sessionsToClose) session->close();
                checkDrainComplete();
            }
        });
}

void HttpServer::startAcceptorLoop()
{
    m_acceptor = std::make_unique<tcp::acceptor>(m_ioc, tcp::endpoint(tcp::v4(), m_port));
    m_logger.log("Server listening on port " + std::to_string(m_port), LogLevel::INFO);

    doAccept();

    unsigned int numIoThreads = std::thread::hardware_concurrency();
    if (numIoThreads == 0)
        numIoThreads = 2;
    if (numIoThreads > 4)
        numIoThreads = 4;

    m_ioThreads.reserve(numIoThreads);
    for (unsigned int i = 0; i < numIoThreads; ++i)
        m_ioThreads.emplace_back([this] { m_ioc.run(); });
}

void HttpServer::doAccept()
{
    if (m_ioc.stopped() || m_draining)
        return;

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

            // Create a session and add it to the storage
            auto session = std::make_shared<Session>(std::move(*socket), m_ioc, *this, *m_processor,
                                                     m_logger, std::chrono::seconds(30));
            {
                std::unique_lock lock(m_sessionsMutex);
                m_sessions.insert(session);
            }
            session->start();
        });
}

// Asynchronous sending of a response
void HttpServer::sendResponse(
    const std::shared_ptr<Session>&                                session,
    std::shared_ptr<http::response<http::string_body>>             response,
    const std::shared_ptr<const http::request<http::string_body>>& request)
{
    // Determining whether to keep the connection open
    bool keepAlive = isKeepAlive(request, boost::system::error_code());
    session->sendResponse(std::move(response), keepAlive);
}

void HttpServer::endSession(const std::shared_ptr<Session>& session)
{
    // Deleting it from the storage
    {
        std::unique_lock lock(m_sessionsMutex);
        m_sessions.erase(session);
    }
    --m_activeSessions;
    m_logger.log("Session ended. Active: " + std::to_string(m_activeSessions.load()),
                 LogLevel::DEBUG);
    if (m_draining)
        checkDrainComplete();
}

void HttpServer::checkDrainComplete()
{
    if (m_draining && m_activeSessions == 0)
    {
        m_logger.log("All sessions finished, stopping I/O", LogLevel::INFO);
        m_ioc.stop();
    }
}

// Determines whether to keep the connection alive after sending a response
bool HttpServer::isKeepAlive(const std::shared_ptr<const http::request<http::string_body>>& request,
                             const boost::system::error_code& ec) noexcept
{
    // If there is a write error or is no request, close the connection
    if (ec || !request)
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
    m_pool.shutdown();

    m_logger.log("Shutting down logger...", LogLevel::INFO);
    m_logger.log("Server stopped.", LogLevel::INFO);
    m_logger.shutdown();
}