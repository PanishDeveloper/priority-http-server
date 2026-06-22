#pragma once

#include "router.hpp"
#include "thread_pool.hpp"

class StatusHandler : public Handler
{
public:
    explicit StatusHandler (const ThreadPool& pool);
    void handle (const boost::beast::http::request<boost::beast::http::string_body>& req,
                 boost::beast::http::response<boost::beast::http::string_body>& res) const override;

private:
    const ThreadPool& m_pool;
};
