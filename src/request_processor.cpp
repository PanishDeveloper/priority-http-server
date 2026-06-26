#include "request_processor.hpp"

#include <algorithm>

#include "server.hpp"
#include "session.hpp"
#include "utils.hpp"

RequestProcessor::RequestProcessor(const Router& router, HttpServer& server, ThreadPool& pool,
                                   boost::asio::io_context& ioc)
    : m_router(router), m_server(server), m_pool(pool), m_ioc(ioc)
{
}

// CPU strategy (computation)
HttpTask::ComputeFn RequestProcessor::createComputeStrategy()
{
    return [](const http::request<http::string_body>& req) -> http::response<http::string_body>
    {
        http::response<http::string_body> res;
        try
        {
            auto json = nlohmann::json::parse(req.body());
            if (!json.is_array())
            {
                utils::makeJsonError(res, "Request body must be an array of integers",
                                     http::status::bad_request);
                return res;
            }

            std::vector<int> numbers = json.get<std::vector<int>>();

            if (numbers.empty())
            {
                utils::makeJsonError(res, "Request body must be an array of integers",
                                     http::status::bad_request);
                return res;
            }
            std::sort(numbers.begin(), numbers.end());
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            nlohmann::json data;
            data["sorted"] = numbers;
            data["size"]   = numbers.size();
            utils::makeJsonSuccess(res, data);
        }
        catch (const std::exception&)
        {
            utils::makeJsonError(res, "Invalid JSON", http::status::bad_request);
        }
        return res;
    };
}

// Callback for completing the task
HttpTask::DoneCallback RequestProcessor::createDoneCallback(
    const std::shared_ptr<Session>&                         session,
    std::shared_ptr<const http::request<http::string_body>> reqPtr) const
{
    return [this, session,
            reqPtr = std::move(reqPtr)](http::response<http::string_body> response) mutable
    { sendResponseAsync(session, std::move(response), reqPtr); };
}

// Asynchronous response sending via io_context
void RequestProcessor::sendResponseAsync(
    const std::shared_ptr<Session>& session, http::response<http::string_body> response,
    std::shared_ptr<const http::request<http::string_body>> reqPtr) const
{
    boost::asio::post(
        m_ioc,
        [session, response = std::move(response), reqPtr = std::move(reqPtr)]() mutable
        {
            auto responsePtr =
                std::make_shared<http::response<http::string_body>>(std::move(response));
            HttpServer::sendResponse(session, responsePtr, reqPtr);
        });
}

// The main method of request processing
void RequestProcessor::process(std::shared_ptr<const http::request<http::string_body>>& reqPtr,
                               int priority, const std::shared_ptr<Session>& session) const
{
    // CPU strategy for POST /compute
    if (reqPtr->method() == http::verb::post && reqPtr->target() == "/compute")
    {
        auto computeFn = createComputeStrategy();
        auto doneCb    = createDoneCallback(session, reqPtr);
        auto task = std::make_unique<HttpTask>(reqPtr, m_server.getLogger(), std::move(computeFn),
                                               std::move(doneCb));
        if (!m_pool.submit(std::move(task), priority))
        {
            http::response<http::string_body> errorRes;
            utils::makeResponse(errorRes, http::status::service_unavailable,
                                "503 Server overloaded", "text/plain");
            sendResponseAsync(session, std::move(errorRes), nullptr);
        }
    }
    else
    {
        // Fast strategy (routing) for all other requests
        http::response<http::string_body> res;
        if (!m_router.route(*reqPtr, res))
            utils::sendNotFound(res);

        res.set(http::field::server, "PriorityHttpServer/1.0");
        res.prepare_payload();

        auto responsePtr = std::make_shared<http::response<http::string_body>>(std::move(res));

        HttpServer::sendResponse(session, responsePtr, reqPtr);
    }
}