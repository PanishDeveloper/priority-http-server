#include "server.hpp"
#include "http_task.hpp"
#include "status_handler.hpp"
#include "static_file_handler.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

HttpServer::HttpServer(unsigned short port, size_t numThreads) : m_port(port), m_pool(numThreads){}

void HttpServer::run()
{
    m_pool.start();
    m_router.addRoute(http::verb::get, "/status", std::make_unique<StatusHandler>(m_pool));
    m_router.addRoute(http::verb::get, "/static/", std::make_unique<StaticFileHandler>("static"));
    try
    {
        tcp::acceptor acceptor(m_ioc, tcp::endpoint(tcp::v4(), m_port));
        std::cout << "Server listening on port " << m_port << "...\n";

        while (true)
        {
            tcp::socket socket(m_ioc);
            acceptor.accept(socket);

            try
            {
                beast::flat_buffer buffer;
                http::request<http::string_body> request;
                http::read(socket, buffer, request);

                int priority = 0;
                auto it = request.find("X-Priority");
                if (it != request.end())
                {
                    try
                    {
                        priority = std::stoi(std::string(it->value()));
                    }catch (const std::exception &) {}
                }

                auto task = std::make_unique<HttpTask>(std::move(socket), std::move(request), m_router);
                m_pool.submit(std::move(task), priority);
            } catch (const std::exception &e)
            {
                std::cerr << "Client request error: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception &e)
    {
        std::cerr << "Server fatal error: " << e.what() << std::endl;
    }
}
