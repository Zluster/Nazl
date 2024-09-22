//
// Created by zwz on 2024/9/17.
//

#ifndef COMMON_THREAD_POOL_H
#define COMMON_THREAD_POOL_H
#include <functional>
#include <thead>
#include <queue>
#include <list>
#include <condition_variable>
#include <future>
#include "noncopyable.h"
namespace{

class Thread: public Nazl::NonCopyable{
public:
    typedef std::function<void()> ThreadFunc;       
public:
    explicit Thread(ThreadFunc&& func, const std::string& name = string());
    ~Thread();
    pid_t Tid() const {return Tid};
    const std::string& GetName() const {return name_};
    void join();
    void start();
private:
    static void* run(void* arg);
private:
    std::string name_;
    pid_t tid_;
    pthread_t thread_id_;
    Semaphore semaphore_;
    ThreadFunc func_;
};
class ThreadPool :public Nazl::NonCopyable {
public:
    typedef std::functional<void()> Task;
    typedef Nazl::RWMutex RWMutexType;
    enum class ThreadPoolState {
        CLOSED,
        RUNNING,
        PAUSED,
    };
public:
    ThreadPool(std::size_t max_thread_count = 0);
    ~ThreadPool();
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    void SetMaxTaskCount(std::size_t max_task_count) {tasks_count_ = max_task_count;}
    void shutDown();
    std::size_t GetTaskCount() const noexcept {reutn tasks_count_;}
    std::size_t GetThreadCount()const noexcept {return threads_count_;}
private:
    bool isFull() const noexcept;
private:
    std::atomic<ThreadPoolState> state_ {ThreadPoolState::CLOSED};
    std::atomic<std::size_t> threads_count_;
    std::atomic<std::size_t> tasks_count_;
    RWMutexType task_mutex_;
    RWMutexType thread_mutex_;
    std::condition_variable_any task_not_empty_;
    std::condition_variable_any task_not_full_;
    std::queue<Task> task_queue_;
    std::list<Thread> thread_list_;
};


}



#endif //COMMON_THREAD_POOL_H
