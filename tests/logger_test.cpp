#include "logger.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "utils.hpp"

class StringSink : public LogSink
{
public:
    void write(const LogMessage& msg) override
    {
        m_stream << '[' << utils::formatTimePoint(msg.timestamp) << ']' << '['
                 << logLevelToStr(msg.level) << "] " << msg.message << std::endl;
    }
    std::string getOutput() const { return m_stream.str(); }

private:
    static const char* logLevelToStr(LogLevel level)
    {
        switch (level)
        {
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
    std::ostringstream m_stream;
};

TEST(AsyncLoggerTest, BasicLogging)
{
    auto        sink    = std::make_unique<StringSink>();
    auto*       rawSink = sink.get();
    AsyncLogger logger(std::move(sink));
    logger.start();

    logger.log("First INFO message", LogLevel::INFO);
    logger.log("Second WARNING message", LogLevel::WARNING);
    logger.log("Third ERROR message", LogLevel::ERROR);

    logger.shutdown();

    std::string output = rawSink->getOutput();
    EXPECT_NE(output.find("First INFO message"), std::string::npos);
    EXPECT_NE(output.find("Second WARNING message"), std::string::npos);
    EXPECT_NE(output.find("Third ERROR message"), std::string::npos);
}

TEST(AsyncLoggerTest, ConcurrentLogging)
{
    auto        sink    = std::make_unique<StringSink>();
    auto*       rawSink = sink.get();
    AsyncLogger logger(std::move(sink));
    logger.start();

    constexpr int            numThreads        = 4;
    constexpr int            messagesPerThread = 25;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&logger, t]
            {
                for (int i = 0; i < messagesPerThread; ++i)
                    logger.log("Thread " + std::to_string(t) + " msg " + std::to_string(i),
                               LogLevel::INFO);
            });
    }

    for (auto& th : threads) th.join();

    logger.shutdown();

    std::string output    = rawSink->getOutput();
    int         lineCount = 0;
    for (char c : output)
        if (c == '\n')
            ++lineCount;
    EXPECT_EQ(lineCount, numThreads * messagesPerThread);
}

TEST(AsyncLoggerTest, ShutdownFlushesAll)
{
    auto        sink    = std::make_unique<StringSink>();
    auto*       rawSink = sink.get();
    AsyncLogger logger(std::move(sink));
    logger.start();

    for (int i = 0; i < 10; ++i) logger.log("Message " + std::to_string(i), LogLevel::INFO);

    logger.shutdown();

    std::string output    = rawSink->getOutput();
    int         lineCount = 0;
    for (char c : output)
        if (c == '\n')
            ++lineCount;
    EXPECT_EQ(lineCount, 10);
}

TEST(AsyncLoggerTest, NoMessagesLostWithoutStart)
{
    auto        sink = std::make_unique<StringSink>();
    AsyncLogger logger(std::move(sink));

    for (int i = 0; i < 5; ++i) logger.log("Message " + std::to_string(i), LogLevel::INFO);

    EXPECT_NO_THROW(logger.shutdown());
}

TEST(AsyncLoggerTest, LevelToString)
{
    auto        sink    = std::make_unique<StringSink>();
    auto*       rawSink = sink.get();
    AsyncLogger logger(std::move(sink));
    logger.start();

    logger.log("Info message", LogLevel::INFO);
    logger.log("Warning message", LogLevel::WARNING);
    logger.log("Error message", LogLevel::ERROR);

    logger.shutdown();

    std::string output = rawSink->getOutput();
    EXPECT_NE(output.find("[INFO]"), std::string::npos);
    EXPECT_NE(output.find("[WARNING]"), std::string::npos);
    EXPECT_NE(output.find("[ERROR]"), std::string::npos);
}