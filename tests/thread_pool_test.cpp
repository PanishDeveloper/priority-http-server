#include "thread_pool.hpp"

#include <gtest/gtest.h>

#include <chrono>

class DammyTask : public Task
{
public:
    explicit DammyTask(int id, std::vector<int>& output, std::mutex& mutex, int sleepMs = 0)
        : m_id(id), m_output(output), m_mutex(mutex), m_sleepMs(sleepMs)
    {
    }
    void execute() override
    {
        if (m_sleepMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleepMs));

        std::lock_guard<std::mutex> lock(m_mutex);
        m_output.emplace_back(m_id);
    }

private:
    int               m_id;
    std::vector<int>& m_output;
    std::mutex&       m_mutex;
    int               m_sleepMs;
};

TEST(ThreadPoolTest, SubmitAndShutdown)
{
    ThreadPool       pool;
    std::vector<int> results;
    std::mutex       mutex;
    pool.start();
    for (int i = 0; i < 20; ++i) pool.submit(std::make_unique<DammyTask>(i, results, mutex));
    pool.shutdown();
    EXPECT_EQ(results.size(), 20);
}

TEST(ThreadPoolTest, PriorityOrder)
{
    ThreadPool pool;
    pool.start();
    std::vector<int> results;
    std::mutex       mutex;

    for (int i = 0; i < 5; ++i)
        pool.submit(std::make_unique<DammyTask>(100 + i, results, mutex, 10), 10);

    for (int i = 0; i < 5; ++i)
        pool.submit(std::make_unique<DammyTask>(300 + i, results, mutex, 60), 0);

    for (int i = 0; i < 5; ++i)
        pool.submit(std::make_unique<DammyTask>(200 + i, results, mutex, 30), 5);
    pool.shutdown();

    EXPECT_EQ(results.size(), 15);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_GE(results[i], 100);
        EXPECT_LE(results[i], 104);
    }
}

TEST(TaskQueueTest, FifoWithSamePriority)
{
    TaskQueue        queue;
    std::vector<int> results;
    std::mutex       mutex;

    queue.push(std::make_unique<DammyTask>(1, results, mutex), 5);
    queue.push(std::make_unique<DammyTask>(2, results, mutex), 5);
    queue.push(std::make_unique<DammyTask>(3, results, mutex), 5);

    auto task1 = queue.pop();
    ASSERT_NE(task1, nullptr);
    task1->execute();

    auto task2 = queue.pop();
    ASSERT_NE(task2, nullptr);
    task2->execute();

    auto task3 = queue.pop();
    ASSERT_NE(task3, nullptr);
    task3->execute();

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
    EXPECT_EQ(results[2], 3);
}

TEST(TaskQueueTest, ShutdownReturnsNull)
{
    TaskQueue        queue;
    std::vector<int> results;
    std::mutex       mutex;

    queue.push(std::make_unique<DammyTask>(42, results, mutex), 0);
    queue.shutdown();

    auto task1 = queue.pop();
    ASSERT_NE(task1, nullptr);
    task1->execute();

    auto task2 = queue.pop();
    ASSERT_EQ(task2, nullptr);
}

TEST(ThreadPoolTest, NoExecutionBeforeStart)
{
    ThreadPool       pool;
    std::vector<int> results;
    std::mutex       mutex;

    for (int i = 0; i < 5; ++i) pool.submit(std::make_unique<DammyTask>(i, results, mutex, 50), 0);

    EXPECT_TRUE(results.empty());

    pool.start();
    pool.shutdown();

    EXPECT_EQ(results.size(), 5);
}

TEST(TaskQueueTest, AgingMakesLowPriorityFirst)
{
    TaskQueue        queue;
    std::vector<int> results;
    std::mutex       mutex;

    queue.push(std::make_unique<DammyTask>(1, results, mutex, 0), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    queue.push(std::make_unique<DammyTask>(2, results, mutex, 0), 10);

    auto task1 = queue.pop();
    task1->execute();
    auto task2 = queue.pop();
    task2->execute();

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
}