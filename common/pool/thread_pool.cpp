//
// Created by zwz on 2024/9/17.
//
#include <iostream>
#include "thread_pool.h"
namespace Nazl
{

Thread::Thread(const Thread::ThreadFunc& func, const std::string &name)
    : name_(name), func_(std::move(func)), thread_id_(0), tid_(0)
{
    if (empty(name_))
    {
        name_ = "thread-unknown";
    }
    if (sem_init(&sem_, 0, 0) != 0)
    {
        throw std::runtime_error("sem_init error");
    }

    int rt = pthread_create(&thread_id_, nullptr, &Thread::run, this);
    if (rt != 0)
    {
        throw std::runtime_error("pthread_create error");
    }
    sem_wait(&sem_);
}

Thread::~Thread()
{
    if (thread_id_ != 0)
    {
        pthread_detach(thread_id_);
    }
    sem_destroy(&sem_);
}

void Thread::join()
{
    if (thread_id_)
    {
        int rt = pthread_join(thread_id_, nullptr);
        if (rt != 0)
        {
            throw std::runtime_error("pthread_join error");
        }
        thread_id_ = 0;
    }
}

void* Thread::run(void* arg)
{
    Thread *p = static_cast<Thread*>(arg);
    p->tid_ = syscall(SYS_gettid);
    pthread_setname_np(p->thread_id_, p->name_.c_str());
    sem_post(&p->sem_);
    ThreadFunc cb;
    cb.swap(p->func_);
    try
    {
        cb();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in thread " << p->name_ << ": " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown exception in thread " << p->name_ << std::endl;
    }
    return nullptr;
}

ThreadPool::ThreadPool(std::size_t max_thread_count)
    : threads_count_(0), tasks_count_(0)
{
    state_ = ThreadPoolState::Running;
    for (std::size_t i = 0; i < max_thread_count; ++i)
    {
        thread_list_.push_back(std::make_shared<Nazl::Thread>(
                                   [this] { this->run(); },
                                   "thread-" + std::to_string(i)
                               ));
        threads_count_++;
    }
}


ThreadPool::~ThreadPool()
{
    shutdown();
}

bool ThreadPool::isFull() noexcept
{
    std::shared_lock<std::shared_mutex> lock(task_mutex_);
    return tasks_count_ > 0 && task_queue_.size() >= tasks_count_;
}

void ThreadPool::shutdown()
{
    if (state_ == ThreadPoolState::Closed)
    {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(thread_mutex_);
    state_ = ThreadPoolState::Closed;
    task_not_empty_.notify_all();
    task_not_full_.notify_all();
    for (auto& thread : thread_list_)
    {
        if (thread)
        {
            thread->join();
        }
    }
}

void ThreadPool::run()
{
    try
    {
        while (state_ == ThreadPoolState::Running)
        {
            Task task;
            {
                std::unique_lock<std::shared_mutex> lock(task_mutex_);
                while (task_queue_.empty() && state_ == ThreadPoolState::Running)
                {
                    task_not_empty_.wait(lock);
                }
                if (!task_queue_.empty() && state_ == ThreadPoolState::Running)
                {
                    task = std::move(task_queue_.front());
                    task_queue_.pop();
                    task_not_full_.notify_one();
                }
            }
            if (task)
            {
                task();
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "ThreadPool::run exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "ThreadPool::run exception: unknown" << std::endl;
    }
}

}