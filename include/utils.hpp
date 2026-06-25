#pragma once

#include <boost/beast/http.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "nlohmann/json.hpp"

namespace http = boost::beast::http;

namespace utils
{
inline void makeResponse(http::response<http::string_body>& res, http::status status,
                         const std::string& body, const std::string& content_type = "text/plain")
{
    res.result(status);
    res.body() = body;
    res.set(http::field::content_type, content_type);
    res.prepare_payload();
}

inline void sendNotFound(http::response<http::string_body>& res)
{
    makeResponse(res, http::status::not_found, "404 Not Found");
}

[[nodiscard]] inline std::string formatTimePoint(const std::chrono::system_clock::time_point& tp)
{
    auto    now = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

inline void makeJsonSuccess(http::response<http::string_body>& res, const nlohmann::json& data,
                            http::status status = http::status::ok)
{
    nlohmann::json response;
    response["status"] = "ok";
    response["data"]   = data;
    makeResponse(res, status, response.dump(), "application/json");
}

inline void makeJsonError(http::response<http::string_body>& res, const std::string& message,
                          http::status status = http::status::bad_request)
{
    nlohmann::json response;
    response["status"]  = "error";
    response["message"] = message;
    makeResponse(res, status, response.dump(), "application/json");
}
}  // namespace utils