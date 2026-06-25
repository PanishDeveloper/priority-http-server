#include "status_handler.hpp"

#include <nlohmann/json.hpp>

#include "utils.hpp"

namespace http = boost::beast::http;

StatusHandler::StatusHandler(const ThreadPool& pool) noexcept : m_pool(pool) {}

void StatusHandler::handle(const http::request<http::string_body>&,
                           http::response<http::string_body>& res) const
{
    nlohmann::json json;
    json["threads"]    = m_pool.threadCount();
    json["queue_size"] = m_pool.pendingTasks();

    utils::makeJsonSuccess(res, json);
}
