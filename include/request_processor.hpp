#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <functional>
#include <memory>

#include "http_task.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

using tcp = boost::asio::ip::tcp;

class HttpServer;  // forward declaration

class RequestProcessor
{
public:
    RequestProcessor(const Router& router, HttpServer& server, ThreadPool& pool,
                     boost::asio::io_context& ioc);
    void process(std::shared_ptr<const http::request<http::string_body>>& reqPtr, int priority,
                 std::shared_ptr<tcp::socket> socket, std::function<void()> restartAccept) const;

private:
    static HttpTask::ComputeFn createComputeStrategy();
    HttpTask::DoneCallback     createDoneCallback(
        const std::shared_ptr<tcp::socket>&                     socket,
        std::shared_ptr<const http::request<http::string_body>> reqPtr,
        std::function<void()>                                   restartAccept) const;

    void sendResponseAsync(const std::shared_ptr<tcp::socket>&                     socket,
                           http::response<http::string_body>                       response,
                           std::shared_ptr<const http::request<http::string_body>> reqPtr,
                           std::function<void()> restartAccept) const;

    const Router&            m_router;
    HttpServer&              m_server;
    ThreadPool&              m_pool;
    boost::asio::io_context& m_ioc;
};