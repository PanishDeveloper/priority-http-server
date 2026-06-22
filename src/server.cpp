#include "server.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/system/errc.hpp>

#include "http_task.hpp"
#include "static_file_handler.hpp"
#include "status_handler.hpp"

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = asio::ip::tcp;

HttpServer::HttpServer (asio::io_context& ioc, unsigned short port,
                        size_t numThreads, std::unique_ptr<LogSink> sink)
    : m_ioc (ioc),
      m_port (port),
      m_pool (numThreads),
      m_logger (std::move (sink)),
      m_signals (m_ioc, SIGINT, SIGTERM)
{
}

void HttpServer::run ()
{
  m_logger.start ();
  m_logger.log ("Server starting...", LogLevel::INFO);

  m_signals.async_wait (
      [this] (const boost::system::error_code& ec, int signal)
      {
        if (!ec)
        {
          m_shutdownRequested = true;
          m_logger.log (
              "Shutdown signal received (SIG " + std::to_string (signal) + ")",
              LogLevel::INFO);
          m_ioc.stop ();
        }
      });

  m_pool.start ();
  m_router.addRoute (http::verb::get, "/status",
                     std::make_unique<StatusHandler> (m_pool));
  m_router.addRoute (http::verb::get, "/static/",
                     std::make_unique<StaticFileHandler> ("static"));

  tcp::acceptor acceptor (m_ioc, tcp::endpoint (tcp::v4 (), m_port));
  m_logger.log ("Server listening on port " + std::to_string (m_port),
                LogLevel::INFO);

  while (!m_shutdownRequested)
  {
    tcp::socket socket (m_ioc);
    try
    {
      acceptor.accept (socket);
    }
    catch (const boost::system::system_error& e)
    {
      if (e.code () == asio::error::operation_aborted ||
          e.code () == boost::system::errc::interrupted)
      {
        m_logger.log ("Accept interrupted by shutdown, exiting...",
                      LogLevel::INFO);
      }
      else
      {
        m_logger.log (std::string ("Accept error: ") + e.what (),
                      LogLevel::ERROR);
      }
      break;
    }
    catch (const std::exception& e)
    {
      m_logger.log (std::string ("Accept error: ") + e.what (),
                    LogLevel::ERROR);
      break;
    }

    try
    {
      beast::flat_buffer               buffer;
      http::request<http::string_body> request;
      http::read (socket, buffer, request);

      int  priority = 0;
      auto it       = request.find ("X-Priority");
      if (it != request.end ())
        try
        {
          priority = std::stoi (std::string (it->value ()));
        }
        catch (const std::exception&)
        {
        }

      auto task = std::make_unique<HttpTask> (
          std::move (socket), std::move (request), m_router, m_logger);
      m_pool.submit (std::move (task), priority);
    }
    catch (boost::system::system_error& e)
    {
      if (e.code () == asio::error::eof ||
          e.code () == http::error::end_of_stream)
        m_logger.log ("Connection closed by client", LogLevel::INFO);
      else
        m_logger.log (std::string ("Client request error: ") + e.what (),
                      LogLevel::ERROR);
    }
    catch (const std::exception& e)
    {
      m_logger.log (std::string ("Client request error: ") + e.what (),
                    LogLevel::ERROR);
    }
  }

  m_logger.log ("Shutting down pool...", LogLevel::INFO);
  m_pool.shutdown ();

  m_logger.log ("Shutting down logger...", LogLevel::INFO);
  m_logger.log ("Server stopped.", LogLevel::INFO);
  m_logger.shutdown ();
}