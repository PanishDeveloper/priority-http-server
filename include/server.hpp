#pragma once
#include <boost/asio/io_context.hpp>

class HttpServer
{
public:
    explicit HttpServer(unsigned short port);
    void run();
private:
    unsigned short m_port;
    boost::asio::io_context m_ioc;
};
