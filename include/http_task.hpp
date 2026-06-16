#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>

#include "thread_pool.hpp"

class HttpTask : public Task
{
public:
    HttpTask(boost::asio::ip::tcp::socket&& socket,
        boost::beast::http::request<boost::beast::http::string_body> request);
    void execute() override;
private:
    boost::asio::ip::tcp::socket m_socket;
    boost::beast::http::request<boost::beast::http::string_body> m_request;
};
