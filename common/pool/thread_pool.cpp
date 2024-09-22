//
// Created by zwz on 2024/9/17.
//

#include "thread_pool.h"
namespace Nazl {

Thread::Thread(Thread::ThreadFunc &&func, const std::string &name)
    : name_(name), func_(func), state_(State::Terminated), thread_id_(0), tid_(0){
    int num = thread_count_++;
    if (empty(name_)) {
        name_ = "thread-" + std::to_string(num);
    }
    int rt = pthread_create(&thread_id_, nullptr, &Thread::run, this);
    if (rt != 0) {
        throw std::runtime_error("pthread_create error");
    }
    semaphore_.wait();
}
Thread::~Thread() {
    if (thread_id_ != 0) {
        pthread_detach(thread_id_);
    }
}
void Thread::join() {
    if (thread_id_) {
        int rt = pthread_join(thread_id_, nullptr);
        if(rt != 0) {
            throw std::runtime_error("pthread_join error");
        }
        thread_id_ = 0;
    }
}
void *Thread::run(void *arg) {
    Thread *p = static_cast<Thread*>(arg);
    p->tid_ = syscall(SYS_gettid);
    pthread_setname_np(p->thread_id_, p->name_.c_str());
    ThreadFunc cb;
    cb.swap(p->func_);
    p->semaphore_.notify();
    cb(); // 执行任务
}
    public:
    ThreadPool(std::size_t max_thread_count = 0);
    ~ThreadPool();
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    void pause();
    void shutdown();
    void terminate();
    void wait();
    std::size_t GetTaskCount();
    std::size_t GetThreadCount();
    private:
    std::atomic<std::size_t> max_task_count;
    RWMutexType task_mutex_;
    RWMutexType work_thread_mutex_;
    std::condition_variable_any task_not_empty_;
    std::condition_variable_any task_not_full_;
    std::queue<Task> task_queue_;
    std::list<Thread> thread_list_;
};
ThreadPool::ThreadPool(std::size_t max_thread_count):
     threads_count_(0)
    , task_count_(0) {
    state_ = State::Running;
    tasks_count_ = max_thread_count;
    task_queue_.clear();
    thread_list_.resize(max_thread_count);
    for (int i = 0; i < max_thread_count; ++i) {
        thread_list_.emplace_back(std::make_shared<Thread>([this](&ThreadPool::run), "thread-" + std::to_string(i)));
        threads_count_++;
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::isFull() {
    RWMutexType::ReadLock lock(task_mutex_);
    return tasks_count_ > 0 && task_queue_.size() >= tasks_count_;
}
template <typename F, typename... Args>
auto submit(F&& f, Args&&... args) {
    if (state_ == State::Terminated) {
        throw std::runtime_error("ThreadPool has been terminated");
    }
    using return_type = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    std::future<return_type> task_future = task->get_future();
    if (thread_list_.empty()) {
        task();
    } else {
        RwMutexType::WriteLock lock(thread_mutex_);
        while (isFull() && state_ == State::Running)  {
            task_not_full_.wait(lock);
            if (state_ == State::Terminated) {
                throw std::runtime_error("ThreadPool has been terminated");
            }
        }
        if (state_ == State::Running) {
            task_queue_.push(std::move(task));
            task_count_++;
            task_not_empty_.notify_one();
        }
    }
    return task_future;
}
void ThreadPool::pause() {
    RWMutexType::WriteLock lock(thread_mutex_);
    state_ = State::Paused;
}
void ThreadPool::shutdown() {
    if (state_ == State::Closed) {
        return;
    }
    RWMutexType::WriteLock lock(thread_mutex_);
    state_ = State::Closed;
    task_not_empty_.notify_all();
    task_not_full_.notify_all();
    for (auto &thread : thread_list_) {
        thtread.join();
    }
}
void ThreadPool::run() {
    try {
        while(state_ == State::Running) {
            Task task;
            {
                RwMutexType::ReadLock lock(task_mutex_);
                while (task_queue_.empty() && state_ == State::Running) {
                    task_not_empty_.wait(lock);
                }
                if (state_ == State::Running) {
                    task = std::move(task_queue_.front());
                    task_queue_.pop();
                    task_count_--;
                    task_not_full_.notify_one();
                }
            }
            if (task) {
                task();
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "ThreadPool::run exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "ThreadPool::run exception: unknown" << std::endl;
    }
}