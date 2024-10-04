//
// Created by zwz on 2024/9/22.
//

#include "timer.h"
namespace Nazl
{
bool Timer::start(long long currentTimeMicros)
{
    timerNode_.when_ = currentTimeMicros + timerNode_.timeout_;
    return true;
}

void Timer::stop()
{
    timerNode_.valid_ = false;
}

TimerMgr::TimerNodeQueue::TimerNodeRefPtr TimerMgr::TimerNodeQueue::next()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return !timerNodeList_.empty();});

    long long currentTimeMicros = TimerMgr::currentTimeMicros();
    auto it = std::find_if(timerNodeList_.begin(), timerNodeList_.end(),
                           [currentTimeMicros](const TimerNodeRefPtr & node)
    {
        return node->when_ <= currentTimeMicros;
    });
    if (it != timerNodeList_.end())
    {
        auto node = *it;
        timerNodeList_.erase(it);
        return node;
    }
    return nullptr;
}
bool TimerMgr::TimerNodeQueue::enqueueNode(const TimerNodeRefPtr& node)
{
    std::lock_guard<std::mutex> lock(mutex_);
    node->valid_ = true;
    timerNodeList_.push_back(node);
    cond_.notify_one();
    return true;
}
void TimerMgr::TimerNodeQueue::removeNode(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(timerNodeList_.begin(), timerNodeList_.end(),
                           [id](const TimerNodeRefPtr & node)
    {
        return node->id_ == id;
    });
    if (it != timerNodeList_.end())
    {
        timerNodeList_.erase(it);
    }
}
bool TimerMgr::TimerNodeQueue::hasNode(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(timerNodeList_.begin(), timerNodeList_.end(),
                           [id](const TimerNodeRefPtr & node)
    {
        return node->id_ == id && node->valid_;
    });
    return it != timerNodeList_.end();
}


TimerMgr::TimerMgr()
    : timerNodeQueue_()
{
    timerTickThread_ = std::thread([this] { processTimerTick(); });
    timerHandlerThread_ = std::thread([this] { processTimerHandler(); });
}

TimerMgr::~TimerMgr()
{
    mainThreadAlive = false;
    cond_.notify_all();
    if (timerTickThread_.joinable())
    {
        timerTickThread_.join();
    }
    if (timerHandlerThread_.joinable())
    {
        timerHandlerThread_.join();
    }
}

Timer TimerMgr::createTimer(long long timeoutMicros, TimerCallback callback, bool periodic)
{
    int id = timerId_.fetch_add(1);
    return Timer(id, timeoutMicros, callback, periodic);
}
void TimerMgr::processTimerTick()
{
    while (mainThreadAlive)
    {
        auto timerNode = timerNodeQueue_.next();
        if (!timerNode || !timerNode->valid_)
        {
            continue;
        }
        if (timerNode->periodic_)
        {
            timerNode->when_ = TimerMgr::currentTimeMicros() + timerNode->timeout_;
            timerNodeQueue_.enqueueNode(timerNode);
        }
        else
        {
            timerNode->valid_ = false;
        }
        timerNodeList_->push_back(timerNode);
        cond_.notify_one();
    }
}
void TimerMgr::processTimerHandler()
{
    while (mainThreadAlive)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !timerNodeQueue_.empty(); });
        auto timerNode = timerNodeQueue_.front();
        timerNodeQueue_.pop_front();
        lock.unlock();

        if (timerNode && timerNode->valid_)
        {
            timerNode->callback_();
        }
    }
}
long long TimerMgr::currentTimeMicros()
{
    auto now = std::chrono::time_point_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now());
    std::chrono::microseconds span =
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    return span.count();
}
bool TimerMgr::start(Timer& timer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& timerNode = timer.getTimerNode();
    if (timerNodeQueue_.hasNode(timerNode.id_))
    {
        timerNodeQueue_.removeNode(timerNode.id_);
    }
    timerNodeQueue_.erase(std::remove(timerNodeQueue_.begin(), timerNodeQueue_.end(),
                                      [&timerNode](const TimerNodeRefPtr & node)
    {
        return node->id_ == timerNode.id_;
    }), timerNodeQueue_.end()));
    timer.start(currentTimeMicros());
    return timerNodeQueue_.enqueueNode(timerNode, timerNode->when_);
}
void TimerMgr::stop(Timer& timer)
{
    MutexType::Lock lock(mutex_);
    const auto& timerNode = timer.getTimerNode();
    timerNodeQueue_.erase(std::remove(timerNodeQueue_.begin(), timerNodeQueue_.end(),
                                      [&timerNode](const TimerNodeRefPtr & node)
    {
        return node->id_ == timerNode.id_;
    }),
    timerNodeQueue_.end()));
    timer.stop();
}
}