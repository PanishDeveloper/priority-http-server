#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>

#include "logger.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

class HttpTask : public Task
{
public:
    HttpTask (boost::asio::ip::tcp::socket&&                               socket,
              boost::beast::http::request<boost::beast::http::string_body> request,
              const Router& router, AsyncLogger& logger);
    void execute () override;

private:
    boost::asio::ip::tcp::socket                                 m_socket;
    boost::beast::http::request<boost::beast::http::string_body> m_request;
    const Router&                                                m_router;
    AsyncLogger&                                                 m_logger;
};
