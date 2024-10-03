//
// Created by zwz on 2024/9/17.
//

#include "thread_pool.h"
namespace Nazl {

Thread::Thread(const Thread::ThreadFunc& func, const std::string &name)
        : name_(name), func_(std::move(func)), thread_id_(0), tid_(0) {
    if (empty(name_)) {
        name_ = "thread-unknown";
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
        if (rt != 0) {
            throw std::runtime_error("pthread_join error");
        }
        thread_id_ = 0;
    }
}

void *Thread::run(void *arg) {
    Thread *p = static_cast<Thread *>(arg);
    p->tid_ = syscall(SYS_gettid);
    pthread_setname_np(p->thread_id_, p->name_.c_str());
    ThreadFunc cb;
    cb.swap(p->func_);
    p->semaphore_.notify();
    cb(); // 执行任务
    return nullptr;
}

ThreadPool::ThreadPool(std::size_t max_thread_count)
    :threads_count_(0), tasks_count_(0) {
    state_ = ThreadPoolState::Running;
    tasks_count_ = max_thread_count;
    thread_list_.resize(max_thread_count);
    for (int i = 0; i < max_thread_count; ++i) {
        thread_list_.emplace_back(std::make_shared<Nazl::Thread>(
                [this] { this->run(); }, // 传递lambda
                "thread-" + std::to_string(i) // 线程名称
        ));
        threads_count_++;
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::isFull() noexcept {
    RWMutexType::ReadLock lock(task_mutex_);
    return tasks_count_ > 0 && task_queue_.size() >= tasks_count_;
}

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args &&... args) -> std::future<decltype(f(args...))> {
    if (state_ == ThreadPoolState::Closed) {
        throw std::runtime_error("ThreadPool has been closed");
    }
    using return_type = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<return_type() >> (
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future <return_type> task_future = task->get_future();
    if (thread_list_.empty()) {
        task();
    } else {
        RWMutexType::WriteLock lock(thread_mutex_);
        while (isFull() && state_ == ThreadPoolState::Running)  {
            task_not_full_.wait(lock);
            if (state_ == ThreadPoolState::Closed) {
                throw std::runtime_error("ThreadPool has been Closed");
            }
        }
        if (state_ == ThreadPoolState::Running) {
            task_queue_.push(std::move(task));
            tasks_count_++;
            task_not_empty_.
            notify_one();

        }
    }
    return task_future;
}

void ThreadPool::shutdown() {
    if (state_ == ThreadPoolState::Closed) {
        return;
    }
    RWMutexType::WriteLock lock(thread_mutex_);
    state_ = ThreadPoolState::Closed;
    task_not_empty_.notify_all();
    task_not_full_.notify_all();
    for (auto &thread: thread_list_) {
        thread->join();
    }
}

void ThreadPool::run() {
    try {
        while (state_ == ThreadPoolState::Running) {
            Task task;
            {
                RWMutexType::ReadLock lock(task_mutex_);
                while (task_queue_.empty() && state_ == ThreadPoolState::Running) {
                    task_not_empty_.wait(lock);
                }
                if (state_ == ThreadPoolState::Running) {
                    task = std::move(task_queue_.front());
                    task_queue_.pop();
                    tasks_count_--;
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

}