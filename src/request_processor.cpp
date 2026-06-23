#include "request_processor.hpp"

#include "server.hpp"
#include "utils.hpp"

RequestProcessor::RequestProcessor(const Router& router, HttpServer& server)
    : m_router(router), m_server(server)
{
}

void RequestProcessor::process(const http::request<http::string_body>& req, int /*priority*/,
                               std::shared_ptr<tcp::socket>            socket,
                               std::function<void()>                   restartAccept) const
{
    http::response<http::string_body> res;

    if (!m_router.route(req, res))
        utils::sendNotFound(res);

    res.set(http::field::server, "PriorityHttpServer/1.0");
    res.prepare_payload();

    auto responsePtr = std::make_shared<http::response<http::string_body>>(std::move(res));

    m_server.sendResponse(std::move(socket), responsePtr, std::make_optional(std::cref(req)),
                          std::move(restartAccept));
}
