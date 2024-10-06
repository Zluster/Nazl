//
// Created by zwz on 2024/10/5.
//
#include <iostream>
#include <future>
#include <vector>
#include "thread_pool.h"
void exampleTask()
{
    std::cout << "Task is running on thread ID: " << ::syscall(SYS_gettid) << std::endl;
    sleep(1); // Simulate work
}
void exampleTask2(int id)
{
    std::cout << "Task " << id << " is running on thread ID: " << ::syscall(SYS_gettid) << std::endl;
    sleep(1); // Simulate work
}
void test_thread()
{
    Nazl::Thread thread(exampleTask, "test1");
    std::cout << "thread_id: " << thread.Tid() << std::endl;
    thread.join();
    Nazl::Thread thread2(std::bind(exampleTask2, 1), "test2");
    std::cout << "thread_id: " << thread2.Tid() << std::endl;
    thread2.join();
}
void simpleTask(int id)
{
    std::cout << "Task " << id << " is starting." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟任务工作
    std::cout << "Task " << id << " is completed." << std::endl;
}

void test_threadpool()
{
    const std::size_t max_threads = 4;
    Nazl::ThreadPool pool(max_threads);

    for (int i = 0; i < 8; ++i)
    {
        pool.submit(simpleTask, i);
    }
    sleep(10);
    pool.shutdown();
    std::cout << "All tasks are completed." << std::endl;

}
int main()
{
    // test_thread();
    test_threadpool();
    return 0;
}