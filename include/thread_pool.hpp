#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
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
    explicit TaskQueue(size_t maxQueueSize = 1000);

    bool                                push(std::unique_ptr<Task> task, int priority = 0);
    [[nodiscard]] std::unique_ptr<Task> pop();
    void                                shutdown();
    [[nodiscard]] size_t                size() const noexcept;
    void                                abort();

private:
    struct PrioritizedTask
    {
        int                                   priority;
        size_t                                order;
        std::unique_ptr<Task>                 task;
        std::chrono::steady_clock::time_point added;
        int                                   effectivePriority;
    };

    struct Compare
    {
        bool operator()(const PrioritizedTask& lhs, const PrioritizedTask& rhs) const
        {
            if (lhs.effectivePriority != rhs.effectivePriority)
                return lhs.effectivePriority < rhs.effectivePriority;
            return lhs.order > rhs.order;
        }
    };

    std::vector<PrioritizedTask>          m_queue;
    mutable std::mutex                    m_mutex;
    std::condition_variable               m_cv;
    bool                                  m_done  = false;
    size_t                                m_order = 0;
    size_t                                m_maxQueueSize;
    static constexpr std::chrono::seconds AGING_THRESHOLD{1};
    void                                  applyAging();
    std::atomic<bool>                     m_abort{false};
};

class WorkerThread
{
public:
    explicit WorkerThread(std::shared_ptr<TaskQueue> queue);
    ~WorkerThread();
    WorkerThread(WorkerThread&&)                 = default;
    WorkerThread& operator=(WorkerThread&&)      = default;
    WorkerThread(const WorkerThread&)            = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;

    void               start();
    void               join();
    bool               joinWithTimeout(std::chrono::milliseconds timeout);
    void               abandon();
    [[nodiscard]] bool isAbandoned() const noexcept { return m_abandoned; }

private:
    struct RunState
    {
        std::shared_ptr<TaskQueue> queue;
        std::promise<void>         donePromise;
    };

    std::shared_ptr<RunState> m_state;
    std::thread               m_thread;
    std::future<void>         m_doneFuture;
    bool                      m_abandoned = false;
};

class ThreadPool
{
public:
    explicit ThreadPool(size_t numThreads   = std::thread::hardware_concurrency(),
                        size_t maxQueueSize = 1000);
    ~ThreadPool();

    [[nodiscard]] bool   submit(std::unique_ptr<Task> task, int priority = 0) const;
    void                 start() const;
    void                 shutdown() const noexcept;
    [[nodiscard]] size_t shutdownWithTimeout(std::chrono::milliseconds timeout) const noexcept;
    [[nodiscard]] size_t threadCount() const { return m_workers.size(); }
    [[nodiscard]] size_t pendingTasks() const { return m_queue->size(); }

private:
    std::shared_ptr<TaskQueue>                 m_queue;
    std::vector<std::unique_ptr<WorkerThread>> m_workers;
};