#include "http_task.hpp"

#include "utils.hpp"

HttpTask::HttpTask(const http::request<http::string_body>& request, AsyncLogger& logger,
                   ComputeFn computeFn, DoneCallback doneCallBack)
    : m_request(request),
      m_logger(logger),
      m_computeFn(std::move(computeFn)),
      m_doneCallBack(std::move(doneCallBack))
{
}

void HttpTask::execute()
{
    auto sendError = [this](const std::string& logMsg)
    {
        m_logger.log(logMsg, LogLevel::ERROR);
        http::response<http::string_body> errorRes;
        utils::makeResponse(errorRes, http::status::internal_server_error,
                            "500 Internal Server Error");
        m_doneCallBack(std::move(errorRes));
    };
    try
    {
        auto response = m_computeFn(m_request);
        m_doneCallBack(std::move(response));
    }
    catch (std::exception& e)
    {
        sendError(std::string("HttpTask error: ") + e.what());
    }
    catch (...)
    {
        sendError("HttpTask unknown error");
    }
}
