#include "server.hpp"

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/system/errc.hpp>
#include <memory>

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
    m_router.addRoute(http::verb::get, "/static/", std::make_unique<StaticFileHandler>("static"));
    m_router.addRoute(http::verb::head, "/static/", std::make_unique<StaticFileHandler>("static"));

    m_pool.start();

    m_signals.async_wait(
        [this](const boost::system::error_code& ec, int signal)
        {
            if (!ec)
            {
                m_logger.log("Shutdown signal received (SIG " + std::to_string(signal) + ")",
                             LogLevel::INFO);
                m_ioc.stop();
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
    if (m_ioc.stopped())
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
            handleSession(socket, []() {});
        });
}

void HttpServer::handleSession(const std::shared_ptr<tcp::socket>& socket,
                               std::function<void()>               restartAccept)
{
    auto buffer = std::make_shared<beast::flat_buffer>();
    auto parser = std::make_shared<http::request_parser<http::string_body>>();

    // Set the body limit (10 MB)
    parser->body_limit(10 * 1024 * 1024);

    auto onReadCompleted = [this, socket, buffer, parser, restartAccept = std::move(restartAccept)](
                               const boost::system::error_code& ec, std::size_t) mutable
    {
        (void)buffer;

        // Handling of reading errors
        if (ec)
        {
            if (ec == http::error::body_limit)
            {
                m_logger.log("Request body too large (413)", LogLevel::WARNING);
                http::response<http::string_body> res;
                utils::makeResponse(res, http::status::payload_too_large, "413 Payload too large");
                auto resPtr = std::make_shared<http::response<http::string_body>>(std::move(res));
                sendResponse(socket, resPtr, nullptr, std::move(restartAccept));
                return;
            }
            if (ec == http::error::end_of_stream || ec == asio::error::eof)
            {
                m_logger.log("Connection closed by client", LogLevel::INFO);
            }
            else if (ec == asio::error::connection_reset)
            {
                m_logger.log("Connection reset by client", LogLevel::DEBUG);
            }
            else
            {
                m_logger.log(std::string("Client request error: ") + ec.message(), LogLevel::ERROR);
            }
            restartAccept();
            return;
        }

        const auto& request = parser->get();

        // Extracting Priority
        int  priority = 0;
        auto it       = request.find("X-Priority");
        if (it != request.end())
        {
            try
            {
                priority = std::stoi(std::string(it->value()));
            }
            catch (...)
            {
                m_logger.log("Invalid priority header, ignoring", LogLevel::WARNING);
            }
        }

        // Sampling priority logging
        if (priority != 0)
        {
            static std::atomic<unsigned int> nonZeroCounter{0};
            unsigned int                     current     = ++nonZeroCounter;
            constexpr unsigned int           SAMPLE_RATE = 10;

            if (current % SAMPLE_RATE == 0)
            {
                m_logger.log("Request priority: " + std::to_string(priority) +
                                 " (sampled, total non-zero=" + std::to_string(current) + ")",
                             LogLevel::INFO);
            }
        }

        auto reqPtr = std::make_shared<const http::request<http::string_body>>(request);
        m_processor->process(reqPtr, priority, socket, std::move(restartAccept));
    };

    http::async_read(*socket, *buffer, *parser, std::move(onReadCompleted));
}

// Asynchronous sending of a response
void HttpServer::sendResponse(std::shared_ptr<tcp::socket>                            socket,
                              std::shared_ptr<http::response<http::string_body>>      response,
                              std::shared_ptr<const http::request<http::string_body>> request,
                              std::function<void()>                                   restartAccept)
{
    http::async_write(*socket, *response,
                      [this, socket = std::move(socket), response = std::move(response),
                       request = std::move(request), restartAccept = std::move(restartAccept)](
                          const boost::system::error_code& ec, std::size_t) mutable
                      {
                          (void)socket;
                          (void)response;

                          if (ec)
                          {
                              m_logger.log(std::string("Write error: ") + ec.message(),
                                           LogLevel::ERROR);
                              restartAccept();
                              return;
                          }

                          // Determining whether to keep the connection
                          if (isKeepAlive(request, ec))
                          {
                              m_logger.log("Keep-Alive: continue", LogLevel::DEBUG);
                              handleSession(socket, restartAccept);
                          }
                          else
                          {
                              m_logger.log("Connection closed", LogLevel::INFO);
                              restartAccept();
                          }
                      });
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
    for (auto& th : m_ioThreads)
        if (th.joinable())
            th.join();

    m_logger.log("Shutting down pool...", LogLevel::INFO);
    m_pool.shutdown();

    m_logger.log("Shutting down logger...", LogLevel::INFO);
    m_logger.log("Server stopped.", LogLevel::INFO);
    m_logger.shutdown();
}