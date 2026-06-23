#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <functional>
#include <memory>

#include "logger.hpp"
#include "router.hpp"

namespace http = boost::beast::http;
using tcp      = boost::asio::ip::tcp;

class HttpServer;  // forward declaration

class RequestProcessor
{
public:
    RequestProcessor(const Router& router, HttpServer& server);
    void process(const http::request<http::string_body>& req, int priority,
                 std::shared_ptr<tcp::socket> socket, std::function<void()> restartAccept) const;

private:
    const Router& m_router;
    HttpServer&   m_server;
};