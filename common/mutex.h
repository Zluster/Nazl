//
// Created by zwz on 2024/9/2.
//

#ifndef COMMON_MUTEX_H
#define COMMON_MUTEX_H

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore>
#include <atomic>
#include "noncopyable.h"
namespace Nazl {

template <typename T>
class ScopedLockImpl{
public:
    ScopedLockImpl(T& mutex):mutex_(mutex) {
        mutex_.lock();
        locked_ = true;
    }
    ~ScopedLockImpl() {
        unlock();
    }
    void lock() {
        if (!locked_) {
            mutex_.lock();
            locked_ = true;
        }
    }

    void unlock() {
        if (locked_) {
            mutex_.unlock();
            locked_ = false;
        }
    }
private:
    T& mutex_;
    bool locked_;
};

class Mutex: public Nazl::Noncopyable {
public:
    typedef ScopedLockImpl<Mutex> Lock;
    Mutex() {
        pthread_mutex_init(&mutex_, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&mutex_);
    }

    void lock() {
        pthread_mutex_lock(&mutex_);
    }

    void unlock() {
        pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_;
};

class Spinlock: public Nazl::Noncopyable {
public:
    typedef ScopedLockImpl<Spinlock> Lock;
    Spinlock() {
        pthread_spin_init(&spinlock_, PTHREAD_PROCESS_PRIVATE);
    }

    ~Spinlock() {
        pthread_spin_destroy(&spinlock_);
    }

    void lock() {
        pthread_spin_lock(&spinlock_);
    }

    void unlock() {
        pthread_spin_unlock(&spinlock_);
    }

private:
    pthread_spinlock_t spinlock_;
};

class CASlock: public Nazl::Noncopyable {
public:
    typedef ScopedLockImpl<CASlock> Lock;
    CASlock() {
        mutex_.clear();
    }
    ~CASlock() {

    }
    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&mutex_, std::memory_order_acquire)) ;
    }
    void unlock() {
        std::atomic_flag_clear_explicit(&mutex_, std::memory_order_release);
    }
private:
    volatile std::atomic_flag mutex_;
};
}




#endif //COMMON_MUTEX_H
