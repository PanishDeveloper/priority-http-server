#include "status_handler.hpp"
#include <nlohmann/json.hpp>

namespace http = boost::beast::http;

StatusHandler::StatusHandler(const ThreadPool& pool) : m_pool(pool) {}

void StatusHandler::handle(const http::request<http::string_body>&, http::response<http::string_body>& res) const
{
    nlohmann::json json;
    json["status"] = "running";
    json["threads"] = m_pool.threadCount();
    json["queue_size"] = m_pool.pendingTasks();

    res.body() = json.dump();
    res.set(http::field::content_type, "application/json");
    res.prepare_payload();
}
