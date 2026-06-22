#include "http_task.hpp"

#include "utils.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;

HttpTask::HttpTask (asio::ip::tcp::socket&& socket, http::request<http::string_body> request,
                    const Router& router, AsyncLogger& logger)
    : m_socket (std::move (socket)),
      m_request (std::move (request)),
      m_router (router),
      m_logger (logger)
{
}

void HttpTask::execute ()
{
    try
    {
        http::response<http::string_body> res;
        if (!m_router.route (m_request, res))
        {
            utils::make_response (res, http::status::not_found, "404 Not found");
        }

        http::serializer<false, http::string_body, http::fields> serializer{res};
        http::write (m_socket, serializer);
    }
    catch (std::exception& e)
    {
        m_logger.log (std::string ("HttpTask error: ") + e.what (), LogLevel::ERROR);
    }
}