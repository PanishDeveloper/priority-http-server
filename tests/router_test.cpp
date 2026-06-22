#include "router.hpp"

#include <gtest/gtest.h>

namespace http = boost::beast::http;

class SpyHandler : public Handler
{
public:
    explicit SpyHandler (bool& called) : m_called (called) {}

    void handle (const http::request<http::string_body>& req,
                 http::response<http::string_body>&      res) const override
    {
        m_called = true;
    }

private:
    bool& m_called;
};

http::request<http::string_body> makeRequest (http::verb method, const std::string& target)
{
    http::request<http::string_body> req;
    req.method (method);
    req.target (target);
    req.version (11);
    return req;
}

TEST (RouterTest, ExactMach)
{
    Router router;
    bool   called = false;

    router.addRoute (http::verb::get, "/test", std::make_unique<SpyHandler> (called));
    http::response<http::string_body> res;

    auto req1 = makeRequest (http::verb::get, "/test");
    EXPECT_TRUE (router.route (req1, res));
    EXPECT_TRUE (called);

    called    = false;
    auto req2 = makeRequest (http::verb::post, "/test");
    EXPECT_FALSE (router.route (req2, res));
    EXPECT_FALSE (called);

    called    = false;
    auto req3 = makeRequest (http::verb::get, "/other");
    EXPECT_FALSE (router.route (req3, res));
    EXPECT_FALSE (called);
}

TEST (RouterTest, PrefixMatch)
{
    Router router;
    bool   called = false;

    router.addRoute (http::verb::get, "/static/", std::make_unique<SpyHandler> (called));
    http::response<http::string_body> res;

    auto req1 = makeRequest (http::verb::get, "/static/");
    EXPECT_TRUE (router.route (req1, res));
    EXPECT_TRUE (called);

    called    = false;
    auto req2 = makeRequest (http::verb::get, "/static/index.html");
    EXPECT_TRUE (router.route (req2, res));
    EXPECT_TRUE (called);

    called    = false;
    auto req3 = makeRequest (http::verb::get, "/static/sub/deep/file.css");
    EXPECT_TRUE (router.route (req3, res));
    EXPECT_TRUE (called);

    called    = false;
    auto req4 = makeRequest (http::verb::get, "/other");
    EXPECT_FALSE (router.route (req4, res));
    EXPECT_FALSE (called);
}

TEST (RouterTest, NotFound)
{
    Router                            router;
    http::response<http::string_body> res;

    auto req = makeRequest (http::verb::get, "/anything");
    EXPECT_FALSE (router.route (req, res));
}

TEST (RouterTest, MultipleHandlers)
{
    Router router;
    bool   called1 = false, called2 = false;

    router.addRoute (http::verb::get, "/one", std::make_unique<SpyHandler> (called1));
    router.addRoute (http::verb::get, "/two", std::make_unique<SpyHandler> (called2));

    http::response<http::string_body> res;

    auto req1 = makeRequest (http::verb::get, "/one");
    EXPECT_TRUE (router.route (req1, res));
    EXPECT_TRUE (called1);
    EXPECT_FALSE (called2);

    called1   = false;
    called2   = false;
    auto req2 = makeRequest (http::verb::get, "/two");
    EXPECT_TRUE (router.route (req2, res));
    EXPECT_FALSE (called1);
    EXPECT_TRUE (called2);
}

TEST (RouterTest, PrefixOverExact)
{
    Router router;
    bool   prefixCalled = false, exactCalled = false;

    router.addRoute (http::verb::get, "/static/", std::make_unique<SpyHandler> (prefixCalled));
    router.addRoute (http::verb::get, "/static/special",
                     std::make_unique<SpyHandler> (exactCalled));

    http::response<http::string_body> res;
    auto                              req = makeRequest (http::verb::get, "/static/special");
    EXPECT_TRUE (router.route (req, res));
    EXPECT_TRUE (exactCalled);
    EXPECT_FALSE (prefixCalled);
}