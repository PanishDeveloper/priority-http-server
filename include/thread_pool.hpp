#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class Task
{
public:
    virtual ~Task()        = default;
    virtual void execute() = 0;
};

class TaskQueue
{
public:
    void                                push(std::unique_ptr<Task> task, int priority = 0);
    [[nodiscard]] std::unique_ptr<Task> pop();
    void                                shutdown();
    [[nodiscard]] size_t                size() const noexcept;

private:
    struct PrioritizedTask
    {
        int                                   priority;
        size_t                                order;
        std::unique_ptr<Task>                 task;
        std::chrono::steady_clock::time_point added;
    };

    struct Compare
    {
        static constexpr std::chrono::seconds AGING_THRESHOLD{1};

        bool operator()(const PrioritizedTask& lhs, const PrioritizedTask& rhs) const
        {
            auto now     = std::chrono::steady_clock::now();
            bool lhsAged = (now - lhs.added) > AGING_THRESHOLD;
            bool rhsAged = (now - rhs.added) > AGING_THRESHOLD;

            if (lhsAged && rhsAged)
            {
                if (lhs.priority != rhs.priority)
                    return lhs.priority < rhs.priority;
                return lhs.order > rhs.order;
            }

            if (lhsAged != rhsAged)
                return !lhsAged;

            if (lhs.priority != rhs.priority)
                return lhs.priority < rhs.priority;
            return lhs.order > rhs.order;
        }
    };

    std::vector<PrioritizedTask> m_queue;
    mutable std::mutex           m_mutex;
    std::condition_variable      m_cv;
    bool                         m_done  = false;
    size_t                       m_order = 0;
};

class WorkerThread
{
public:
    explicit WorkerThread(TaskQueue& queue) noexcept;
    ~WorkerThread();
    WorkerThread(WorkerThread&&) noexcept            = default;
    WorkerThread& operator=(WorkerThread&&) noexcept = default;

    void start();
    void join();

private:
    TaskQueue*  m_queue;
    std::thread m_thread;
};

class ThreadPool
{
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void                 submit(std::unique_ptr<Task> task, int priority = 0);
    void                 start();
    void                 shutdown() noexcept;
    [[nodiscard]] size_t threadCount() const { return m_workers.size(); }
    [[nodiscard]] size_t pendingTasks() const { return m_queue.size(); }

private:
    TaskQueue                 m_queue;
    std::vector<WorkerThread> m_workers;
};