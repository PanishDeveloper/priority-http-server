#include "thread_pool.hpp"

#include <iostream>

// Realization of methods TaskQueue
void TaskQueue::push (std::unique_ptr<Task> task, int priority)
{
    {
        std::lock_guard<std::mutex> lock (m_mutex);
        m_queue.push (PrioritizedTask{priority, m_order++, std::move (task)});
    }
    m_cv.notify_one ();
}

std::unique_ptr<Task> TaskQueue::pop ()
{
    std::unique_lock<std::mutex> lock (m_mutex);
    m_cv.wait (lock, [this] { return !m_queue.empty () || m_done; });

    if (!m_queue.empty ())
    {
        auto task = std::move (m_queue.top ().task);
        m_queue.pop ();
        return task;
    }

    return nullptr;
}

void TaskQueue::shutdown ()
{
    {
        std::lock_guard<std::mutex> lock (m_mutex);
        m_done = true;
    }
    m_cv.notify_all ();
}

size_t TaskQueue::size () const
{
    std::lock_guard<std::mutex> lock (m_mutex);
    return m_queue.size ();
}

// Realization of methods WorkerThread
WorkerThread::WorkerThread (TaskQueue& queue) : m_queue (&queue) {}

WorkerThread::~WorkerThread ()
{
    if (m_thread.joinable ())
        m_thread.join ();
}

void WorkerThread::start ()
{
    m_thread = std::thread (
        [this]
        {
            while (true)
            {
                auto task = m_queue->pop ();
                if (!task)
                    break;
                task->execute ();
            }
        });
}

void WorkerThread::join ()
{
    if (m_thread.joinable ())
        m_thread.join ();
}

// Realization of methods ThreadPool
ThreadPool::ThreadPool (size_t numThreads)
{
    if (numThreads == 0)
        numThreads = 1;

    m_workers.reserve (numThreads);
    try
    {
        for (size_t i = 0; i < numThreads; ++i) m_workers.emplace_back (m_queue);
    }
    catch (...)
    {
        m_queue.shutdown ();
        throw;
    }
}

ThreadPool::~ThreadPool ()
{
    shutdown ();
}

void ThreadPool::submit (std::unique_ptr<Task> task, int priority)
{
    m_queue.push (std::move (task), priority);
}

void ThreadPool::start ()
{
    for (auto& w : m_workers) w.start ();
}

void ThreadPool::shutdown ()
{
    m_queue.shutdown ();
    for (auto& w : m_workers) w.join ();
}