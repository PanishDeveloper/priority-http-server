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
    void push(std::unique_ptr<Task> task);
    std::unique_ptr<Task> pop();
    void shutdown();
private:
    std::queue<std::unique_ptr<Task>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_done = false;
};

class WorkerThread
{
public:
    explicit WorkerThread(TaskQueue& queue);
    ~WorkerThread();
    WorkerThread(WorkerThread&&) = default;
    WorkerThread& operator=(WorkerThread&&) noexcept = default;

    void join();
private:
    std::thread m_thread;
};

class ThreadPool
{
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void submit(std::unique_ptr<Task> task);
    void shutdown();
private:
    TaskQueue m_queue;
    std::vector<WorkerThread> m_workers;
};