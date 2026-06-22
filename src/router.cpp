#include "router.hpp"

namespace http = boost::beast::http;

// Realization of methods RouterTable
std::string RouterTable::makeKey(http::verb method, const std::string& path)
{
    return std::string(http::to_string(method)) + " " + path;
}

void RouterTable::addRoute(http::verb method, const std::string& path,
                           std::unique_ptr<Handler> handler)
{
    m_routes[makeKey(method, path)] = std::move(handler);
}

Handler* RouterTable::findHandler(http::verb method, const std::string& path) const
{
    auto it = m_routes.find(makeKey(method, path));
    if (it != m_routes.end())
        return it->second.get();
    return nullptr;
}

Handler* RouterTable::findPrefixHandler(http::verb method, const std::string& path) const
{
    std::string fullPath = makeKey(method, path);

    Handler* best       = nullptr;
    size_t   bestLength = 0;

    for (const auto& [key, handler] : m_routes)
    {
        if (fullPath.rfind(key, 0) == 0)
        {
            if (key.length() > bestLength)
            {
                bestLength = key.length();
                best       = handler.get();
            }
        }
    }
    return best;
}

// Realization of methods Router
void Router::addRoute(http::verb method, const std::string& path, std::unique_ptr<Handler> handler)
{
    m_table.addRoute(method, path, std::move(handler));
}

bool Router::route(const http::request<http::string_body>& req,
                   http::response<http::string_body>&      res) const
{
    std::string path    = std::string(req.target());
    auto*       handler = m_table.findHandler(req.method(), path);

    if (!handler)
        handler = m_table.findPrefixHandler(req.method(), path);

    if (!handler)
        return false;
    handler->handle(req, res);
    return true;
}