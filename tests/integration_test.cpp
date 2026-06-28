#include <gtest/gtest.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
using tcp       = asio::ip::tcp;

// An auxiliary function for making an HTTP request and receiving a responcse
http::response<http::string_body> doRequest(http::verb method, const std::string& target,
                                            const std::string& body        = "",
                                            const std::string& contentType = "")
{
    try
    {
        asio::io_context            ioc;
        tcp::resolver               resolver(ioc);
        tcp::resolver::results_type results = resolver.resolve("localhost", "8080");
        tcp::socket                 socket(ioc);
        asio::connect(socket, results);

        http::request<http::string_body> req{method, target, 11};
        req.set(http::field::host, "localhost");
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        if (!body.empty())
        {
            req.body() = body;
            req.set(http::field::content_type,
                    contentType.empty() ? "application/json" : contentType);
            req.prepare_payload();
        }

        http::write(socket, req);

        beast::flat_buffer                buffer;
        http::response<http::string_body> res;
        http::read(socket, buffer, res);

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);

        return res;
    }
    catch (const std::exception&)
    {
        // If you are unable to connect, skip the test
        http::response<http::string_body> res;
        res.result(static_cast<http::status>(0));
        return res;
    }
}

// We check that the server is available
bool isServerAlive()
{
    auto res = doRequest(http::verb::get, "/status");
    return res.result_int() != 0;
}

// Tests
TEST(IntegrationTest, StatusEndpoint)
{
    if (!isServerAlive())
        GTEST_SKIP() << "Server is not running on localhost:8080";

    auto res = doRequest(http::verb::get, "/status");
    EXPECT_EQ(res.result(), http::status::ok);

    auto json = nlohmann::json::parse(res.body());

    EXPECT_TRUE(json.contains("status"));
    EXPECT_EQ(json["status"], "ok");
    EXPECT_TRUE(json.contains("data"));
    EXPECT_TRUE(json["data"].contains("threads"));
    EXPECT_TRUE(json["data"].contains("queue_size"));
    EXPECT_TRUE(json["data"]["threads"].is_number());
    EXPECT_TRUE(json["data"]["queue_size"].is_number());
}

TEST(IntegrationTest, StaticFileSmall)
{
    if (!isServerAlive())
        GTEST_SKIP() << "Server is not running on localhost:8080";

    auto res = doRequest(http::verb::get, "/static/index.html");
    EXPECT_EQ(res.result(), http::status::ok);
    EXPECT_EQ(res[http::field::content_type], "text/html");
    EXPECT_FALSE(res.body().empty());
}

TEST(IntegrationTest, StaticFileTooLarge)
{
    if (!isServerAlive())
        GTEST_SKIP() << "Server is not running on localhost:8080";

    // Check if the big.bin file exists in the static folder
    if (!std::filesystem::exists("static/big.bin"))
        GTEST_SKIP() << "File static/big.bin not found, skipping 413 test";

    auto res = doRequest(http::verb::get, "/static/big.bin");
    EXPECT_EQ(res.result(), http::status::payload_too_large);
    EXPECT_EQ(res.body(), "413 Payload Too Large");
}

TEST(IntegrationTest, ComputeValidJson)
{
    if (!isServerAlive())
        GTEST_SKIP() << "Server is not running on localhost:8080";

    std::string jsonBody = "[5,2,8,1]";
    auto        res      = doRequest(http::verb::post, "/compute", jsonBody, "application/json");

    EXPECT_EQ(res.result(), http::status::ok);

    auto json = nlohmann::json::parse(res.body());

    EXPECT_EQ(json["status"], "ok");
    EXPECT_TRUE(json.contains("data"));
    EXPECT_TRUE(json["data"].contains("sorted"));
    auto sorted = json["data"]["sorted"].get<std::vector<int>>();
    EXPECT_EQ(sorted, (std::vector<int>{1, 2, 5, 8}));
    EXPECT_EQ(json["data"]["size"], 4);
}

TEST(IntegrationTest, ComputeInvalidJson)
{
    if (!isServerAlive())
        GTEST_SKIP() << "Server is not running on localhost:8080";

    std::string invalidJson = "{not json";
    auto        res = doRequest(http::verb::post, "/compute", invalidJson, "application/json");

    EXPECT_EQ(res.result(), http::status::bad_request);
    auto json = nlohmann::json::parse(res.body());
    EXPECT_EQ(json["status"], "error");
    EXPECT_EQ(json["message"], "Invalid JSON");
}