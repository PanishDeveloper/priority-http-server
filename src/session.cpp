#include "session.hpp"

#include <tuple>

#include "server.hpp"
#include "utils.hpp"

Session::Session(tcp::socket socket, asio::io_context& ioc, HttpServer& server,
                 RequestProcessor& processor, AsyncLogger& logger, std::chrono::seconds timeout)
    : m_socket(std::move(socket)),
      m_timer(ioc),
      m_server(server),
      m_processor(processor),
      m_logger(logger),
      m_timeout(timeout),
      m_strand(asio::make_strand(ioc))
{
}

void Session::start()
{
    m_server.incrementSessions();
    m_logger.log("Session started. Active: " + std::to_string(m_server.getActiveSessions()),
                 LogLevel::DEBUG);
    auto self = shared_from_this();
    asio::post(m_strand, [self] { self->doRead(); });
}

void Session::doRead()
{
    if (m_closed)
        return;

    m_parser.emplace();
    m_parser->body_limit(10 * 1024 * 1024);

    m_timer.expires_after(m_timeout);
    m_timer.async_wait(
        asio::bind_executor(m_strand,
                            [self = shared_from_this()](boost::system::error_code ec)  // NOLINT
                            { self->onTimeout(ec); }));

    http::async_read(m_socket, m_buffer, *m_parser,
                     asio::bind_executor(
                         m_strand, [self = shared_from_this()](const boost::system::error_code& ec,
                                                               size_t) { self->onRead(ec); }));
}

void Session::onRead(const boost::system::error_code& ec)
{
    m_timer.cancel();

    if (ec)
    {
        if (ec == http::error::end_of_stream || ec == asio::error::eof)
            m_logger.log("Connection closed by client", LogLevel::INFO);
        else if (ec == asio::error::operation_aborted)
            m_logger.log("Read aborted (shutdown or timeout)", LogLevel::DEBUG);
        else if (ec == http::error::body_limit)
        {
            m_logger.log("Request body too large (413)", LogLevel::WARNING);
            http::response<http::string_body> res;
            utils::makeResponse(res, http::status::payload_too_large, "413 Payload too large");
            auto resPtr = std::make_shared<http::response<http::string_body>>(std::move(res));
            sendResponse(resPtr, false);
            return;
        }
        else if (ec == asio::error::connection_reset)
            m_logger.log("Connection reset by peer", LogLevel::DEBUG);
        else
            m_logger.log(std::string("Read error: ") + ec.message(), LogLevel::ERROR);

        endSession();
        return;
    }

    const auto& request = m_parser->get();

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
            m_logger.log("Invalid priority header", LogLevel::WARNING);
        }
    }

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
    m_processor.process(reqPtr, priority, shared_from_this());
}

void Session::onWrite(const boost::system::error_code& ec)
{
    if (ec)
    {
        if (ec == asio::error::broken_pipe)
            m_logger.log("Write error: broken pipe (client closed connection)", LogLevel::DEBUG);
        else
            m_logger.log(std::string("Write error: ") + ec.message(), LogLevel::ERROR);
        endSession();
        return;
    }

    if (!m_keepAlive || m_server.isDraining())
    {
        if (m_server.isDraining())
            m_logger.log("Draining: closing connection", LogLevel::INFO);
        else
            m_logger.log("Connection closed", LogLevel::INFO);
        endSession();
        return;
    }

    m_logger.log("Keep-Alive: continue", LogLevel::DEBUG);
    doRead();
}

void Session::onTimeout(const boost::system::error_code& ec)
{
    if (ec == asio::error::operation_aborted)
        return;
    m_logger.log("Read timeout, closing session", LogLevel::INFO);
    endSession();
}

void Session::close()
{
    if (m_closed)
        return;
    m_closed = true;
    m_timer.cancel();
    boost::system::error_code ec;
    std::ignore = m_socket.shutdown(tcp::socket::shutdown_both, ec);  // подавляем предупреждение
    std::ignore = m_socket.close(ec);
    endSession();
}

void Session::endSession()
{
    if (m_ended)
        return;
    m_ended = true;
    m_server.endSession(shared_from_this());
}

void Session::sendResponse(std::shared_ptr<http::response<http::string_body>> response,
                           bool                                               keepAlive)
{
    m_keepAlive = keepAlive;
    if (m_closed)
        return;
    http::async_write(
        m_socket, *response,
        asio::bind_executor(
            m_strand, [self = shared_from_this(), response = std::move(response)](
                          const boost::system::error_code& ec, size_t) { self->onWrite(ec); }));
}