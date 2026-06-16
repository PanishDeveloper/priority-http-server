#include "http_task.hpp"

#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

HttpTask::HttpTask(boost::asio::ip::tcp::socket&& socket,
        boost::beast::http::request<boost::beast::http::string_body> request) : m_socket(std::move(socket)),
                                                                                m_request(std::move(request)) {}

void HttpTask::execute()
{
    try {
        std::string body = "Hello from priority HTTP server";
        http::response<http::string_body> res{http::status::ok, m_request.version()};
        res.body() = body;
        res.set(http::field::content_type, "text/plain");
        res.set(http::field::server, "PriorityServer/1.0");
        res.prepare_payload();

        http::serializer<false, http::string_body, http::fields> serializer{res};
        http::write(m_socket, serializer);
    } catch (std::exception& e)
    {
        std::cerr << "HttpTask error: " << e.what() << std::endl;
    }
}