//#include "server.hpp"
#include "thread_pool.hpp"

#include <iostream>
#include <chrono>

class TestTask : public Task
{
public:
    explicit TestTask(int id) : m_id(id) {}
    void execute() override
    {
        std::cout << "Task " << m_id << " thread " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
private:
    int m_id;
};

int main()
{
    // std::cout << "Server starting..." << std::endl;
    // HttpServer server(8080);
    // server.run();

    ThreadPool pool;
    for (int i = 0; i < 20; ++i)
    {
        pool.submit(std::make_unique<TestTask>(i));
    }
    pool.shutdown();
    std::cout << "All task done." << std::endl;

    return 0;
}
