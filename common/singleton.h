//
// Created by zwz on 2024/9/10.
//

#ifndef COMMON_SINGLETON_H
#define COMMON_SINGLETON_H
#include <memory>
namespace Nazl{
template<typename T>
class Singleton{
public:
    static T& getInstance(){
        static T instance;
        return instance;
    }
private:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    ~Singleton() = default;
};
template<typename T>
class SingletonPtr{
    static std::shared_ptr<T> getInstance(){
        static std::shared_ptr<T> instance(new T());
        return instance;
    }
private:
    SingletonPtr() = default;
    SingletonPtr(const SingletonPtr&) = delete;
    SingletonPtr& operator=(const SingletonPtr&) = delete;
    ~SingletonPtr() = default;
};
};
#endif //COMMON_SINGLETON_H
