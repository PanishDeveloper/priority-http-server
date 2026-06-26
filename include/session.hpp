#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <memory>

#include "logger.hpp"
#include "request_processor.hpp"

namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;

class HttpServer;  // forward declaration

// The Session class manages a single HTTP connection
class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, asio::io_context& ioc, HttpServer& server,
            RequestProcessor& processor, AsyncLogger& logger, std::chrono::seconds timeout);
    // Starts reading the first request
    void start();
    // Forcibly closes the session
    void close();
    // Sends a response
    void sendResponse(std::shared_ptr<http::response<http::string_body>> response, bool keepAlive);

private:
    void doRead();
    void onRead(const boost::system::error_code& ec);
    void onWrite(const boost::system::error_code& ec);
    void onTimeout(const boost::system::error_code& ec);
    void endSession();

    tcp::socket                                              m_socket;
    beast::flat_buffer                                       m_buffer;
    boost::optional<http::request_parser<http::string_body>> m_parser;
    asio::steady_timer                                       m_timer;
    HttpServer&                                              m_server;
    RequestProcessor&                                        m_processor;
    AsyncLogger&                                             m_logger;
    std::chrono::seconds                                     m_timeout;
    bool                                                     m_keepAlive = false;
    bool                                                     m_closed    = false;
    bool                                                     m_ended     = false;
    asio::strand<asio::io_context::executor_type>            m_strand;
};
