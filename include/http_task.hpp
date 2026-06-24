#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>

#include "logger.hpp"
#include "thread_pool.hpp"

namespace http = boost::beast::http;

class HttpTask : public Task
{
public:
    using ComputeFn =
        std::function<http::response<http::string_body>(const http::request<http::string_body>&)>;
    using DoneCallback = std::function<void(http::response<http::string_body>)>;

    HttpTask(std::shared_ptr<const http::request<http::string_body>> request, AsyncLogger& logger,
             ComputeFn computeFn, DoneCallback doneCallBack);

    void execute() override;

private:
    std::shared_ptr<const http::request<http::string_body>> m_request;
    AsyncLogger&                                            m_logger;
    ComputeFn                                               m_computeFn;
    DoneCallback                                            m_doneCallBack;
};
