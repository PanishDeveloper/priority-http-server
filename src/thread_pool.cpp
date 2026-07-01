#include "thread_pool.hpp"

#include <iostream>
#include <limits>

// Realization of methods TaskQueue
TaskQueue::TaskQueue(size_t maxQueueSize) : m_maxQueueSize(maxQueueSize) {}

bool TaskQueue::push(std::unique_ptr<Task> task, int priority)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_done || m_queue.size() >= m_maxQueueSize)
            return false;

        m_queue.push_back(PrioritizedTask{priority, m_order++, std::move(task),
                                          std::chrono::steady_clock::now(), priority});
        std::push_heap(m_queue.begin(), m_queue.end(), Compare{});
    }
    m_cv.notify_one();
    return true;
}

std::unique_ptr<Task> TaskQueue::pop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_queue.empty() || m_done || m_abort; });
    if (m_abort)
        return nullptr;

    if (!m_queue.empty())
    {
        applyAging();
        std::pop_heap(m_queue.begin(), m_queue.end(), Compare{});
        auto task = std::move(m_queue.back().task);
        m_queue.pop_back();
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

size_t TaskQueue::size() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void TaskQueue::applyAging()
{
    auto now     = std::chrono::steady_clock::now();
    bool anyAged = false;

    for (auto& item : m_queue)
    {
        if ((now - item.added) > AGING_THRESHOLD)
        {
            item.effectivePriority = std::numeric_limits<int>::max();
            anyAged                = true;
        }
    }

    if (anyAged)
    {
        std::make_heap(m_queue.begin(), m_queue.end(), Compare{});
    }
}

void TaskQueue::abort()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_abort = true;
    }
    m_cv.notify_all();
}

// Realization of methods WorkerThread
WorkerThread::WorkerThread(std::shared_ptr<TaskQueue> queue) : m_state(std::make_shared<RunState>())
{
    m_state->queue = std::move(queue);
    m_doneFuture   = m_state->donePromise.get_future();
}

WorkerThread::~WorkerThread()
{
    if (!m_abandoned)
        join();
}

void WorkerThread::start()
{
    auto state = m_state;
    m_thread   = std::thread(
        [state]
        {
            while (true)
            {
                auto task = state->queue->pop();
                if (!task)
                    break;
                task->execute();
            }
            state->donePromise.set_value();
        });
}

void WorkerThread::join()
{
    if (m_thread.joinable())
        m_thread.join();
}

bool WorkerThread::joinWithTimeout(std::chrono::milliseconds timeout)
{
    if (m_abandoned)
        return false;

    if (m_doneFuture.wait_for(timeout) != std::future_status::ready)
        return false;

    join();
    return true;
}

void WorkerThread::abandon()
{
    if (m_thread.joinable())
        m_thread.detach();
    m_abandoned = true;
}

// Realization of methods ThreadPool
ThreadPool::ThreadPool(size_t numThreads, size_t maxQueueSize)
    : m_queue(std::make_shared<TaskQueue>(maxQueueSize))
{
    if (numThreads == 0)
        numThreads = 1;

    m_workers.reserve(numThreads);
    try
    {
        for (size_t i = 0; i < numThreads; ++i)
            m_workers.emplace_back(std::make_unique<WorkerThread>(m_queue));
    }
    catch (...)
    {
        m_queue->shutdown();
        throw;
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

bool ThreadPool::submit(std::unique_ptr<Task> task, int priority) const
{
    return m_queue->push(std::move(task), priority);
}

void ThreadPool::start() const
{
    for (auto& w : m_workers) w->start();
}

void ThreadPool::shutdown() const noexcept
{
    m_queue->shutdown();
    for (auto& w : m_workers) w->join();
}

size_t ThreadPool::shutdownWithTimeout(std::chrono::milliseconds timeout) const noexcept
{
    m_queue->shutdown();

    if (timeout.count() <= 0)
    {
        for (auto& w : m_workers) w->join();
        return 0;
    }

    constexpr auto kAbortGrace = std::chrono::milliseconds(500);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    std::vector<WorkerThread*> stillPending;
    for (auto& w : m_workers)
    {
        if (w->isAbandoned())
            continue;

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining < std::chrono::milliseconds(0))
            remaining = std::chrono::milliseconds(0);

        if (!w->joinWithTimeout(remaining))
            stillPending.push_back(w.get());
    }

    if (stillPending.empty())
        return 0;

    m_queue->abort();
    auto graceDeadline = std::chrono::steady_clock::now() + kAbortGrace;

    size_t abandoned = 0;
    for (auto* w : stillPending)
    {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            graceDeadline - std::chrono::steady_clock::now());
        if (remaining < std::chrono::milliseconds(0))
            remaining = std::chrono::milliseconds(0);

        if (!w->joinWithTimeout(remaining))
        {
            ++abandoned;
            w->abandon();
            std::cerr << "[ThreadPool] WARNING: worker thread did not finish within "
                      << timeout.count() << "ms (+" << kAbortGrace.count()
                      << "ms abort grace) and was abandoned" << std::endl;
        }
    }

    return abandoned;
}