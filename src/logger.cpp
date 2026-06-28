#include "logger.hpp"

#include <iostream>

#include "utils.hpp"

namespace
{
std::string formatLogMessage(const LogMessage& msg)
{
    std::ostringstream oss;
    oss << '[' << utils::formatTimePoint(msg.timestamp) << ']' << '[' << logLevelToStr(msg.level)
        << "] " << msg.message;
    return oss.str();
}

std::string toUpper(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}
}  // namespace

const char* logLevelToStr(LogLevel level)
{
    switch (level)
    {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

LogLevel stringToLogLevel(const std::string& level)
{
    std::string upper = toUpper(level);
    if (upper == "DEBUG")
        return LogLevel::DEBUG;
    if (upper == "INFO")
        return LogLevel::INFO;
    if (upper == "WARNING")
        return LogLevel::WARNING;
    if (upper == "ERROR")
        return LogLevel::ERROR;
    return LogLevel::INFO;
}

// Realization of methods MessageQueue
void MessageQueue::push(const LogMessage& msg)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(msg);
    }
    m_cv.notify_one();
}

std::optional<LogMessage> MessageQueue::pop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]() { return !m_queue.empty() || m_done; });

    if (!m_queue.empty())
    {
        LogMessage msg = m_queue.front();
        m_queue.pop();
        return msg;
    }

    return std::nullopt;
}

void MessageQueue::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_done = true;
    }
    m_cv.notify_all();
}

size_t MessageQueue::size() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

// Realization of methods ConsoleSink
void ConsoleSink::write(const LogMessage& msg)
{
    std::cout << formatLogMessage(msg) << std::endl;
}

// Realization of methods FileSink
FileSink::FileSink(const std::string& filePath)
{
    m_file.open(filePath, std::ios::out | std::ios::app);
    if (!m_file.is_open())
    {
        throw std::runtime_error("Failed to open log file: " + filePath);
    }
}

FileSink::~FileSink()
{
    if (m_file.is_open())
        m_file.close();
}

void FileSink::write(const LogMessage& msg)
{
    m_file << formatLogMessage(msg) << std::endl;
    m_file.flush();
}

// Realization of methods LogConsumer
LogConsumer::LogConsumer(MessageQueue& queue, LogSink& sink) : m_queue(queue), m_sink(sink) {}

LogConsumer::~LogConsumer()
{
    if (m_thread.joinable())
        m_thread.join();
}

void LogConsumer::start()
{
    m_thread = std::thread(
        [this]
        {
            while (true)
            {
                auto msg = m_queue.pop();
                if (!msg.has_value())
                    break;
                m_sink.write(*msg);
            }
        });
}

void LogConsumer::join()
{
    if (m_thread.joinable())
        m_thread.join();
}

// Realization of methods AsyncLogger
AsyncLogger::AsyncLogger(std::unique_ptr<LogSink> sink)
    : m_sink(std::move(sink)), m_consumer(m_queue, *m_sink)
{
}

AsyncLogger::~AsyncLogger()
{
    shutdown();
}

void AsyncLogger::log(const std::string& message, LogLevel level)
{
    if (level < m_minLevel)
        return;
    m_queue.push(LogMessage(message, level));
}

void AsyncLogger::start()
{
    m_consumer.start();
}

void AsyncLogger::shutdown()
{
    m_queue.shutdown();
    m_consumer.join();
}
