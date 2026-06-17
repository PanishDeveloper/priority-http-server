#pragma once

#include <boost/beast/http.hpp>
#include <memory>
#include <unordered_map>
#include <string>

class Handler
{
public:
    virtual ~Handler() = default;
    virtual void handle(const boost::beast::http::request<boost::beast::http::string_body>& req,
                        boost::beast::http::response<boost::beast::http::string_body>& res) const = 0;
};

class RouterTable
{
public:
    void addRoute(boost::beast::http::verb method, const std::string& path, std::unique_ptr<Handler> handler);
    [[nodiscard]] Handler* findHandler(boost::beast::http::verb method, const std::string& path) const;

private:
    std::unordered_map<std::string, std::unique_ptr<Handler>> m_routes;
};

class Router
{
public:
    Router() = default;

    void addRoute(boost::beast::http::verb method, const std::string& path, std::unique_ptr<Handler> handler);
    bool route(const boost::beast::http::request<boost::beast::http::string_body>& req,
               boost::beast::http::response<boost::beast::http::string_body>& res) const;

private:
    RouterTable m_table;
};