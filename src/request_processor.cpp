#include "request_processor.hpp"

#include <algorithm>

#include "server.hpp"
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
    const std::shared_ptr<tcp::socket>&                     socket,
    std::shared_ptr<const http::request<http::string_body>> reqPtr,
    std::function<void()>                                   restartAccept) const
{
    return [this, socket, reqPtr = std::move(reqPtr), restartAccept = std::move(restartAccept)](
               http::response<http::string_body> response) mutable
    { sendResponseAsync(socket, std::move(response), reqPtr, std::move(restartAccept)); };
}

// Asynchronous response sending via io_context
void RequestProcessor::sendResponseAsync(
    const std::shared_ptr<tcp::socket>& socket, http::response<http::string_body> response,
    std::shared_ptr<const http::request<http::string_body>> reqPtr,
    std::function<void()>                                   restartAccept) const
{
    boost::asio::post(
        m_ioc,
        [this, socket, response = std::move(response), reqPtr = std::move(reqPtr),
         restartAccept = std::move(restartAccept)]() mutable
        {
            auto responsePtr =
                std::make_shared<http::response<http::string_body>>(std::move(response));
            m_server.sendResponse(socket, responsePtr, reqPtr, std::move(restartAccept));
        });
}

// The main method of request processing
void RequestProcessor::process(std::shared_ptr<const http::request<http::string_body>>& reqPtr,
                               int priority, std::shared_ptr<tcp::socket> socket,
                               std::function<void()> restartAccept) const
{
    // CPU strategy for POST /compute
    if (reqPtr->method() == http::verb::post && reqPtr->target() == "/compute")
    {
        auto computeFn = createComputeStrategy();
        auto doneCb    = createDoneCallback(socket, reqPtr, std::move(restartAccept));
        auto task = std::make_unique<HttpTask>(reqPtr, m_server.getLogger(), std::move(computeFn),
                                               std::move(doneCb));
        m_pool.submit(std::move(task), priority);
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

        m_server.sendResponse(std::move(socket), responsePtr, reqPtr, std::move(restartAccept));
    }
}