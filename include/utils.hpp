#pragma once

#include <boost/beast/http.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace http = boost::beast::http;

namespace utils
{
inline void make_response(http::response<http::string_body>& res, http::status status,
                          const std::string& body, const std::string& content_type = "text/plain")
{
    res.result(status);
    res.body() = body;
    res.set(http::field::content_type, content_type);
    res.prepare_payload();
}

inline void sendNotFound(http::response<http::string_body>& res)
{
    make_response(res, http::status::not_found, "404 Not Found");
}

inline std::string formatTimePoint(const std::chrono::system_clock::time_point& tp)
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
}  // namespace utils