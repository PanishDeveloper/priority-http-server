//#include "server.hpp"
#include "thread_pool.hpp"

#include <iostream>
#include <chrono>

class TestTask : public Task
{
public:
    explicit TestTask(int id, int prio) : m_id(id), m_prio(prio) {}
    void execute() override
    {
        std::cout << "Task id = " << m_id << " priority = " << m_prio <<
            " thread = " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
private:
    int m_id;
    int m_prio;
};

int main()
{
    // std::cout << "Server starting..." << std::endl;
    // HttpServer server(8080);
    // server.run();

    ThreadPool pool(2);
    std::vector<std::pair<int, int>> tasks {
        {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1},
        {6, 10}, {7, 10}, {8, 10}, {9, 10}, {10, 10},
        {11, 5}, {12, 5}, {13, 5}, {14, 5}, {15, 5}
    };

    for (auto& [id, prio] : tasks)
    {
        pool.submit(std::make_unique<TestTask>(id, prio), prio);
    }

    pool.start();
    pool.shutdown();
    std::cout << "All task done." << std::endl;

    return 0;
}
