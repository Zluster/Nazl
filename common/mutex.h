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

class Semaphore : public Nazl::Noncopable {
public:
    Semaphore(int32_t count = 0) {
        if (sem_init(&sem_, 0, count)) {
            throw std::runtime_error("sem_init failed");
        }
    }
    ~Semaphore() {
        if (sem_destroy(&sem_)) {
            throw std::runtime_error("sem_destroy failed");
        }
    }
    void notify() {
        if (sem_post(&sem_)) {
            throw std::runtime_error("sem_post failed");
        }
    }
    void wait() {
        if (sem_wait(&sem_)) {
            throw std::runtime_error("sem_wait failed");
        }
    }
private:
    sem_t sem_;
};


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
template <class T>
class ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T& mutex):mutex_(mutex) {
        mutex_.rdlock();
        locked_ = true;
    }
    ~ReadScopedLockImpl() {
        unlock();
    }
    void lock() {
        if (!locked_) {
            mutex_.rdlock();
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

template <class T>
class WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T& mutex):mutex_(mutex) {
        mutex_.wrlock();
        locked_ = true;
    }
    ~WriteScopedLockImpl() {
        unlock();
    }
    void lock() {
        if (!locked_) {
            mutex_.wrlock();
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

class RWMutex: public Nazl::Noncopyable {
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    RWMutex() {
        pthread_rwlock_init(&rwlock_, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&rwlock_);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&rwlock_);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&rwlock_);
    }

    void unlock() {
        pthread_rwlock_unlock(&rwlock_);
    }

private:
    pthread_rwlock_t rwlock_;
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
class ConditionVariable {
public:
    ConditionVariable() {
        if (pthread_cond_init(&cond_, nullptr)) {
            throw std::runtime_error("pthread_cond_init failed");
        }
        if (pthread_mutex_init(&mutex_, nullptr)) {
            throw std::runtime_error("pthread_mutex_init failed");
        }
    }

    ~ConditionVariable() {
        if (pthread_cond_destroy(&cond_)) {
            throw std::runtime_error("pthread_cond_destroy failed");
        }
        if (pthread_mutex_destroy(&mutex_)) {
            throw std::runtime_error("pthread_mutex_destroy failed");
        }
    }

    void wait() {
        if (pthread_mutex_lock(&mutex_)) {
            throw std::runtime_error("pthread_mutex_lock failed");
        }
        if (pthread_cond_wait(&cond_, &mutex_)) {
            pthread_mutex_unlock(&mutex_);
            throw std::runtime_error("pthread_cond_wait failed");
        }
        if (pthread_mutex_unlock(&mutex_)) {
            throw std::runtime_error("pthread_mutex_unlock failed");
        }
    }

    void notify() {
        if (pthread_cond_signal(&cond_)) {
            throw std::runtime_error("pthread_cond_signal failed");
        }
    }

    void notifyAll() {
        if (pthread_cond_broadcast(&cond_)) {
            throw std::runtime_error("pthread_cond_broadcast failed");
        }
    }

private:
    pthread_cond_t cond_;
    pthread_mutex_t mutex_;
};
}




#endif //COMMON_MUTEX_H
