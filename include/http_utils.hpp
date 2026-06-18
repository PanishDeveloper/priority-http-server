#pragma once

#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

inline void make_response(http::response<http::string_body>& res, http::status status,
                                const std::string& body, const std::string& content_type = "text/plain")
{
    res.result(status);
    res.body() = body;
    res.set(http::field::content_type, content_type);
    res.prepare_payload();
}
