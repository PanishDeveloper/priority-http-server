#pragma once

#include <condition_variable>
#include <memory>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>

class Task
{
public:
    virtual ~Task() = default;
    virtual void execute() = 0;
};

class TaskQueue
{
public:
    void push(std::unique_ptr<Task> task, int priority = 0);
    std::unique_ptr<Task> pop();
    void shutdown();
    [[nodiscard]] size_t size() const;

private:
    struct PrioritizedTask
    {
        int priority;
        size_t order;
        mutable std::unique_ptr<Task> task;
    };

    struct Compare
    {
        bool operator()(const PrioritizedTask& lhs, const PrioritizedTask& rhs) const
        {
            if (lhs.priority != rhs.priority)
                return lhs.priority < rhs.priority;

            return lhs.order > rhs.order;
        }
    };

    std::priority_queue<PrioritizedTask, std::vector<PrioritizedTask>, Compare> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_done = false;
    size_t m_order = 0;
};

class WorkerThread
{
public:
    explicit WorkerThread(TaskQueue& queue);
    ~WorkerThread();
    WorkerThread(WorkerThread&&) = default;
    WorkerThread& operator=(WorkerThread&&) noexcept = default;

    void start();
    void join();

private:
    TaskQueue* m_queue;
    std::thread m_thread;
};

class ThreadPool
{
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void submit(std::unique_ptr<Task> task, int priority = 0);
    void start();
    void shutdown();
    inline size_t threadCount() const { return m_workers.size(); }
    inline size_t pendingTasks() const { return m_queue.size(); }

private:
    TaskQueue m_queue;
    std::vector<WorkerThread> m_workers;
};