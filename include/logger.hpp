#pragma once

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

enum class LogLevel
{
    INFO,
    WARNING,
    ERROR
};

struct LogMessage
{
    std::string                           message;
    std::chrono::system_clock::time_point timestamp;
    LogLevel                              level = LogLevel::INFO;

    LogMessage() = default;
    LogMessage(std::string msg, LogLevel lvl)
        : message(std::move(msg)), timestamp(std::chrono::system_clock::now()), level(lvl)
    {
    }
};

class MessageQueue
{
public:
    void                 push(const LogMessage& msg);
    LogMessage           pop();
    void                 shutdown();
    [[nodiscard]] size_t size() const;

private:
    std::queue<LogMessage>  m_queue;
    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
    bool                    m_done = false;
};

class LogSink
{
public:
    virtual ~LogSink()                        = default;
    virtual void write(const LogMessage& msg) = 0;
};

class ConsoleSink : public LogSink
{
public:
    void write(const LogMessage& msg) override;
};

class FileSink : public LogSink
{
public:
    explicit FileSink(const std::string& filePath);
    ~FileSink() override;
    void write(const LogMessage& msg) override;

private:
    std::ofstream m_file;
};

class LogConsumer
{
public:
    explicit LogConsumer(MessageQueue& queue, LogSink& sink);
    ~LogConsumer();

    void start();
    void join();

private:
    MessageQueue& m_queue;
    LogSink&      m_sink;
    std::thread   m_thread;
};

class AsyncLogger
{
public:
    explicit AsyncLogger(std::unique_ptr<LogSink> sink);
    ~AsyncLogger();

    void log(const std::string& message, LogLevel level = LogLevel::INFO);
    void start();
    void shutdown();

private:
    MessageQueue             m_queue;
    std::unique_ptr<LogSink> m_sink;
    LogConsumer              m_consumer;
};
