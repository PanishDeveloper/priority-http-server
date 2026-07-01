#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "thread_pool.hpp"

namespace
{
class StuckTask : public Task
{
public:
    explicit StuckTask(std::atomic<bool>& started) : m_started(started) {}

    void execute() override
    {
        m_started = true;
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

private:
    std::atomic<bool>& m_started;
};

class QuickTask : public Task
{
public:
    explicit QuickTask(std::atomic<int>& counter) : m_counter(counter) {}
    void execute() override { ++m_counter; }

private:
    std::atomic<int>& m_counter;
};
}  // namespace

TEST(ThreadPoolStuckWorkerTest, ShutdownWithTimeoutAbandonsStuckWorkerAndReturns)
{
    constexpr auto kShutdownTimeout = std::chrono::milliseconds(300);
    constexpr auto kMaxAllowedWall  = std::chrono::seconds(5);

    ThreadPool pool(/*numThreads=*/1, /*maxQueueSize=*/10);
    pool.start();

    std::atomic<bool> started{false};
    ASSERT_TRUE(pool.submit(std::make_unique<StuckTask>(started)));

    for (int i = 0; i < 100 && !started.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(started.load()) << "Worker never picked up the stuck task";

    auto   wallStart   = std::chrono::steady_clock::now();
    size_t abandoned   = pool.shutdownWithTimeout(kShutdownTimeout);
    auto   wallElapsed = std::chrono::steady_clock::now() - wallStart;

    EXPECT_EQ(abandoned, 1u)
        << "Expected exactly one abandoned (detached) worker for the stuck task";
    EXPECT_LT(wallElapsed, kMaxAllowedWall)
        << "shutdownWithTimeout() took too long — possible regression to a hang";
}

TEST(ThreadPoolStuckWorkerTest, HealthyWorkersFinishWhileOneIsStuck)
{
    ThreadPool pool(/*numThreads=*/4, /*maxQueueSize=*/100);
    pool.start();

    std::atomic<bool> stuckStarted{false};
    std::atomic<int>  quickCounter{0};

    ASSERT_TRUE(pool.submit(std::make_unique<StuckTask>(stuckStarted)));
    for (int i = 0; i < 50; ++i)
        ASSERT_TRUE(pool.submit(std::make_unique<QuickTask>(quickCounter)));

    size_t abandoned = pool.shutdownWithTimeout(std::chrono::milliseconds(500));

    EXPECT_EQ(abandoned, 1u);
    EXPECT_EQ(quickCounter.load(), 50)
        << "All quick tasks should complete even though one worker is stuck";
}

TEST(ThreadPoolStuckWorkerTest, SecondShutdownCallAfterAbandonIsSafeAndFast)
{
    ThreadPool pool(/*numThreads=*/1, /*maxQueueSize=*/10);
    pool.start();

    std::atomic<bool> started{false};
    ASSERT_TRUE(pool.submit(std::make_unique<StuckTask>(started)));
    for (int i = 0; i < 100 && !started.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(started.load());

    size_t firstAbandoned = pool.shutdownWithTimeout(std::chrono::milliseconds(200));
    EXPECT_EQ(firstAbandoned, 1u);

    auto   wallStart         = std::chrono::steady_clock::now();
    size_t secondAbandoned   = pool.shutdownWithTimeout(std::chrono::milliseconds(200));
    auto   secondWallElapsed = std::chrono::steady_clock::now() - wallStart;

    EXPECT_EQ(secondAbandoned, 0u);
    EXPECT_LT(secondWallElapsed, std::chrono::seconds(1));
}