//
// Created by zwz on 2024/9/22.
//

#ifndef NAZL_TIMER_H
#define NAZL_TIMER_H
#include <functional>
#include <queue>
#include <string>
#include <list>
#include <map>
#include <condition_variable>
#include "mutex.h"
namespace Nazl
{
class Timer
{
public:
    typedef std::function<void()> TimerCallback;
    struct TimerNode
    {
        int id_;
        long long timeout_;
        TimerCallback callback_;
        bool periodic_;
        long long when_;
        bool valid_;

        TimerNode(int id, long long timeout, TimerCallback callback, bool periodic = false)
            : id_(id), timeout_(timeout), callback_(callback), periodic_(periodic), when_(0), valid_(true) {}
    };

    Timer(int id, long long timeout, TimerCallback callback, bool periodic)
        : timerNode_(id, timeout * 1000, callback, periodic) {}

    bool start(long long currentTimeMicros);
    void stop();
    const TimerNode& getTimerNode() const
    {
        return timerNode_;
    }

private:
    TimerNode timerNode_;
};

class TimerMgr
{
public:
    typedef std::function<void()> TimerCallback;
    class TimerNodeQueue
    {
    public:
        using TimerNodeRefPtr = std::shared_ptr<Timer::TimerNode>;
        using TimerNodeRefPtrList = std::deque<TimerNodeRefPtr>;
        TimerNodeQueue() = default;
        ~TimerNodeQueue() = default;
        TimerNodeRefPtr next();
        bool enqueueNode(const TimerNodeRefPtr& node);
        void removeNode(int id);
        bool hasNode(int id);

    private:
        TimerNodeRefPtrList timerNodeList_;
        std::mutex mutex_;
        std::condition_variable cond_;
    };

    TimerMgr();
    ~TimerMgr();
    Timer createTimer(long long timeoutMicros, TimerCallback callback, bool periodic = false);
    static long long currentTimeMicros();
    bool start(Timer& timer);
    void stop(Timer& timer);

private:
    TimerNodeQueue timerNodeQueue_;
    std::shared_ptr<TimerNodeQueue::TimerNodeRefPtrList> timerNodeList_;
    std::atomic<bool> mainThreadAlive{true};
    std::thread timerTickThread_;
    std::thread timerHandlerThread_;
    std::atomic<size_t> timerId_{0};
    std::mutex mutex_;
    std::condition_variable cond_;
    void processTimerTick();
    void processTimerHandler();
};

}


#endif //NAZL_TIMER_H
