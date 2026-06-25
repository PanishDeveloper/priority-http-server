#pragma once

#include <boost/beast/http.hpp>
#include <memory>
#include <string>
#include <unordered_map>

class Handler
{
public:
    virtual ~Handler() = default;
    virtual void handle(
        const boost::beast::http::request<boost::beast::http::string_body>& req,
        boost::beast::http::response<boost::beast::http::string_body>&      res) const = 0;
};

class RouterTable
{
public:
    [[nodiscard]] static std::string makeKey(boost::beast::http::verb method,
                                             const std::string&       path);
    void                   addRoute(boost::beast::http::verb method, const std::string& path,
                                    std::unique_ptr<Handler> handler);
    [[nodiscard]] Handler* findHandler(boost::beast::http::verb method,
                                       const std::string&       path) const;
    [[nodiscard]] Handler* findPrefixHandler(boost::beast::http::verb method,
                                             const std::string&       path) const;

private:
    std::unordered_map<std::string, std::unique_ptr<Handler>> m_routes;
};

class Router
{
public:
    Router() = default;

    void               addRoute(boost::beast::http::verb method, const std::string& path,
                                std::unique_ptr<Handler> handler);
    [[nodiscard]] bool route(
        const boost::beast::http::request<boost::beast::http::string_body>& req,
        boost::beast::http::response<boost::beast::http::string_body>&      res) const;

private:
    RouterTable m_table;
};