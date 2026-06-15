#include "server.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

HttpServer::HttpServer(unsigned short port) : m_port(port){}

void HttpServer::run()
{
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

                std::string body = "Hello from priority HTTP server";
                http::response<http::string_body> res{http::status::ok, request.version()};
                res.body() = body;
                res.set(http::field::content_type, "text/plain");
                res.set(http::field::server, "PriorityServer/1.0");
                res.prepare_payload();

                http::serializer<false, http::string_body, http::fields> serializer{res};
                http::write(socket, serializer);
            } catch (const std::exception &e)
            {
                std::cerr << "Client error: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception &e)
    {
        std::cerr << "Server fatal error: " << e.what() << std::endl;
    }
}
