//
// Created by zwz on 2024/9/22.
//

#ifndef NAZL_TIMER_H
#define NAZL_TIMER_H
#include <functional>
#include <queue>
#include <string>
#include "mutex.h"
namespace Nazl{
class Timer {
public:
    typedef std::function<void()> TimerCallback;
    Timer(int id. lonhg long timeout, TimerCallback callback, bool periodic)
        :timerNode_(id, timeout * 1000, callback, periodic) {}
    bool start(long long currentTimeMicros);
    void stop();
    const TimerNode& getTimerNode() const { return timerNode_; }
private:
    struct TimerNode {
        const int id_;
        const long long timeout_;
        const TimerCallback callback_;
        bool periodic_;
        long long when_;
        bool valid_;
        TimerNode(const int id, const long long timeout, const TimerCallback callback, bool periodic = false)
                : id_(id), timeout_(timeout), callback_(callback), periodic_(periodic), when_(0), valid_(true)
        {}
    };

    TimerNode timerNode_;

};

class TimerMgr {
public:
    typedef Nazl::Mutex MutexType;
    using TimerNodeRefPtr = std::shared_ptr<Timer::TimerNode>;
    using TimerNodeRefPtrList = std::deque<TimerNodeRefPtr>;

    TimerMgr();
    ~TimerMgr();
    Timer createTimer(long long timeoutMicros, TimerCallback callback, bool periodic = false);
    static long long currentTimeMicros();
    bool start(Timer& timer);
    void stop(Timer& timer);
private:
    class TimerNodeQueue{
    public:
        TimerNodeQueue() = default;
        ~TimerNodeQueue() = default;
        TimerNodeRefPtr next();
        bool enqueueNode(const TimerNodeRefPtr& node);
        void removeNode(int id);
        bool hasNode(int id) const;
    private:
        TimerNodeRefPtrList timerNodeList_ {};
        std::map<int, std::list<long long>> timerIdWhenMap_ {};
        MutexType mutex_;
        std::condition_variable cond_;
        void waitForItems();
        void showNode() const;
    };
    TimeNodeQueue timerNodeQueue_;
    std::shared_ptr<TimerNodeRefPtrList> timerNodeList_ = std::make_shared<TimerNodeRefPtrList>();
    std::atomic<bool> mainThreadAlive{true};
    std::thread timerTickThread_;
    std::thread timerHandlerThread_;
    std::atomic<size_t> timerId_{0};
    MutexType mutex_;
    std::condition_variable cond_;
    void processTimerTick();
    void processTimerHandler();
};

}


#endif //NAZL_TIMER_H
