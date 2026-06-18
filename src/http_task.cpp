#include "http_task.hpp"
#include "http_utils.hpp"

#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;

HttpTask::HttpTask(asio::ip::tcp::socket&& socket, http::request<http::string_body> request, const Router& router)
                   : m_socket(std::move(socket)), m_request(std::move(request)), m_router(router) {}

void HttpTask::execute()
{
    try {
        http::response<http::string_body> res;
        if (!m_router.route(m_request, res))
        {
            make_response(res, http::status::not_found, "404 Not found");
        }

        http::serializer<false, http::string_body, http::fields> serializer{res};
        http::write(m_socket, serializer);
    } catch (std::exception& e)
    {
        std::cerr << "HttpTask error: " << e.what() << std::endl;
    }
}