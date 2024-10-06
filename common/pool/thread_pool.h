//
// Created by zwz on 2024/9/17.
//

#ifndef COMMON_THREAD_POOL_H
#define COMMON_THREAD_POOL_H
#include <functional>
#include <thread>
#include <queue>
#include <list>
#include <condition_variable>
#include <future>
#include <string>
#include <unistd.h>
#include <sys/syscall.h>
#include <shared_mutex>
#include <semaphore.h>
#include "noncopyable.h"
namespace Nazl
{

class Thread: public Nazl::Noncopyable
{
public:
    typedef std::function<void()> ThreadFunc;
public:
    explicit Thread(const ThreadFunc& func, const std::string& name = "");
    ~Thread();
    pid_t Tid() const
    {
        return static_cast<pid_t>(::syscall(SYS_gettid));
    };
    const std::string& GetName() const
    {
        return name_;
    }
    void join();
private:
    static void* run(void* arg);
private:
    std::string name_;
    pid_t tid_;
    pthread_t thread_id_;
    sem_t sem_;
    ThreadFunc func_;
};
class ThreadPool : public Nazl::Noncopyable
{
public:
    typedef std::function<void()> Task;
    enum class ThreadPoolState
    {
        Closed,
        Running,
        Paused,
    };
public:
    ThreadPool(std::size_t max_thread_count = 0);
    ~ThreadPool();
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    void setMaxTaskCount(std::size_t max_task_count)
    {
        tasks_count_ = max_task_count;
    }
    void shutdown();
    void run();
    std::size_t GetTaskCount() const noexcept
    {
        return tasks_count_;
    }
    std::size_t GetThreadCount()const noexcept
    {
        return threads_count_;
    }
private:
    bool isFull() noexcept;
private:
    std::atomic<ThreadPoolState> state_ {ThreadPoolState::Closed};
    std::atomic<std::size_t> threads_count_;
    std::atomic<std::size_t> tasks_count_;
    std::shared_mutex task_mutex_;
    std::shared_mutex thread_mutex_;
    std::condition_variable_any task_not_empty_;
    std::condition_variable_any task_not_full_;
    std::queue<Task> task_queue_;
    std::list<std::shared_ptr<Nazl::Thread>> thread_list_;
};
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
{
    if (state_ == ThreadPoolState::Closed)
    {
        throw std::runtime_error("ThreadPool has been closed");
    }
    using return_type = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<return_type()>>(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> task_future = task->get_future();
    {
        std::unique_lock<std::shared_mutex> lock(task_mutex_);
        if (state_ == ThreadPoolState::Running)
        {
            task_queue_.push([task]()
            {
                (*task)();
            });
            task_not_empty_.notify_one();
        }
    }
    return task_future;
}

}



#endif //COMMON_THREAD_POOL_H
