#include "thread_pool.hpp"

#include <iostream>

// Realization of methods TaskQueue
void TaskQueue::push(std::unique_ptr<Task> task)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push(std::move(task));
    }
    m_cv.notify_one();
}

std::unique_ptr<Task> TaskQueue::pop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_tasks.empty() || m_done; });

    if (!m_tasks.empty())
    {
        auto task = std::move(m_tasks.front());
        m_tasks.pop();
        return task;
    }

    return nullptr;
}

void TaskQueue::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_done = true;
    }
    m_cv.notify_all();
}


// Realization of methods WorkerThread
WorkerThread::WorkerThread(TaskQueue& queue)
{
    m_thread = std::thread([&queue]
    {
        while (true)
        {
            auto task = queue.pop();
            if (!task) break;
            task->execute();
        }
    });
}

WorkerThread::~WorkerThread()
{
    if (m_thread.joinable())
        m_thread.join();
}

void WorkerThread::join()
{
    if (m_thread.joinable())
        m_thread.join();
}


// Realization of methods ThreadPool
ThreadPool::ThreadPool(size_t numThreads)
{
    if (numThreads == 0) numThreads = 1;

    m_workers.reserve(numThreads);
    try
    {
        for (size_t i = 0; i < numThreads; ++i)
            m_workers.emplace_back(m_queue);
    } catch (...)
    {
        m_queue.shutdown();
        throw;
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::submit(std::unique_ptr<Task> task)
{
    m_queue.push(std::move(task));
}

void ThreadPool::shutdown()
{
    m_queue.shutdown();
    for (auto& w : m_workers) w.join();
}