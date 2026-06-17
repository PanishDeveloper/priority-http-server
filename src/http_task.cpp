#include "http_task.hpp"

#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

HttpTask::HttpTask(asio::ip::tcp::socket&& socket, http::request<http::string_body> request, const Router& router)
                   : m_socket(std::move(socket)), m_request(std::move(request)), m_router(router) {}

void HttpTask::execute()
{
    try {
        http::response<http::string_body> res{http::status::ok, m_request.version()};
        if (!m_router.route(m_request, res))
        {
            res.result(http::status::not_found);
            res.body() = "404 Not Found";
            res.set(http::field::content_type, "text/plain");
            res.prepare_payload();
        }

        http::serializer<false, http::string_body, http::fields> serializer{res};
        http::write(m_socket, serializer);
    } catch (std::exception& e)
    {
        std::cerr << "HttpTask error: " << e.what() << std::endl;
    }
}