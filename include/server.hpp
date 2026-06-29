#pragma once

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <shared_mutex>
#include <unordered_set>

#include "config.hpp"
#include "logger.hpp"
#include "request_processor.hpp"
#include "router.hpp"
#include "thread_pool.hpp"

namespace http = boost::beast::http;

class Session;  // forward declaration

class HttpServer
{
public:
    explicit HttpServer(boost::asio::io_context& ioc, const Config& config);
    ~HttpServer();

    void run();
    void setLogLevel(LogLevel level) noexcept { m_logger.setMinLevel(level); }
    void sendResponse(const std::shared_ptr<Session>&                                session,
                      std::shared_ptr<http::response<http::string_body>>             response,
                      const std::shared_ptr<const http::request<http::string_body>>& request) const;
    // Method for ending the session
    void                       endSession(const std::shared_ptr<Session>& session);
    size_t                     getActiveSessions() const { return m_activeSessions.load(); }
    bool                       isDraining() const noexcept { return m_draining.load(); }
    [[nodiscard]] AsyncLogger& getLogger() { return m_logger; }
    const Config&              getConfig() const noexcept { return m_config; }

private:
    void setup();
    void startAcceptorLoop();
    void shutdown();
    void doAccept();
    void checkDrainComplete();

    [[nodiscard]] static bool isKeepAlive(
        const std::shared_ptr<const http::request<http::string_body>>& request,
        const boost::system::error_code&                               ec) noexcept;

    boost::asio::io_context&                        m_ioc;
    Config                                          m_config;
    ThreadPool                                      m_pool;
    Router                                          m_router;
    AsyncLogger                                     m_logger;
    boost::asio::signal_set                         m_signals;
    std::vector<std::thread>                        m_ioThreads;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
    std::unique_ptr<RequestProcessor>               m_processor;
    std::unordered_set<std::shared_ptr<Session>>    m_sessions;
    mutable std::shared_mutex                       m_sessionsMutex;
    std::atomic<bool>                               m_draining{false};
    std::atomic<size_t>                             m_activeSessions{0};
};