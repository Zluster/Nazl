
#ifndef NAZL_TIMER_H
#define NAZL_TIMER_H

#include <functional>
#include <queue>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <iostream>
#include <memory>

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
            : id_(id), timeout_(timeout), callback_(callback), periodic_(periodic), when_(0), valid_(false) {}
    };

    Timer(int id, long long timeout, TimerCallback callback, bool periodic)
        : timerNode_(std::make_shared<TimerNode>(id, timeout, callback, periodic)) {}

    bool start(long long currentTimeMicros);
    void stop();
    std::shared_ptr<TimerNode> getTimerNode() const
    {
        return timerNode_;
    }
    bool isValid() const
    {
        return timerNode_ && timerNode_->valid_;
    }

private:
    std::shared_ptr<TimerNode> timerNode_;
};

class TimerMgr
{
public:
    typedef std::function<void()> TimerCallback;
    using TimerNodeRefPtr = std::shared_ptr<Timer::TimerNode>;

    TimerMgr();
    ~TimerMgr();
    template<typename Rep, typename Period>
    Timer createTimer(const std::chrono::duration<Rep, Period>& timeout, TimerCallback callback, bool periodic = false)
    {
        auto timeoutMicros = std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
        return createTimer(timeoutMicros, std::move(callback), periodic);
    }
    Timer createTimer(long long timeoutMicros, TimerCallback callback, bool periodic = false);
    static long long currentTimeMicros();
    bool start(Timer& timer);
    void stop(Timer& timer);
    void removeTimer(int id);
    size_t getActiveTimerCount() const;

private:
    struct CompareTimerNode
    {
        bool operator()(const TimerNodeRefPtr& lhs, const TimerNodeRefPtr& rhs) const
        {
            return lhs->when_ > rhs->when_;
        }
    };

    void enqueueNode(const TimerNodeRefPtr& node);
    TimerNodeRefPtr next();
    void processTimerHandler();

private:
    std::priority_queue<TimerNodeRefPtr, std::vector<TimerNodeRefPtr>, CompareTimerNode> queue_;
    std::unordered_map<int, TimerNodeRefPtr> nodeMap_;
    std::atomic<bool> mainThreadAlive{true};
    std::thread timerHandlerThread_;
    std::atomic<size_t> timerId_{0};
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

} // namespace Nazl

#endif // NAZL_TIMER_H  