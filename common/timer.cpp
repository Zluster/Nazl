#include "timer.h"
#include <chrono>

namespace Nazl
{

bool Timer::start(long long currentTimeMicros)
{
    timerNode_->when_ = currentTimeMicros + timerNode_->timeout_;
    timerNode_->valid_ = true;
    return true;
}

void Timer::stop()
{
    timerNode_->valid_ = false;
}

TimerMgr::TimerMgr()
{
    mainThreadAlive = true;
    timerHandlerThread_ = std::thread([this] { processTimerHandler(); });
}

TimerMgr::~TimerMgr()
{
    mainThreadAlive = false;
    cond_.notify_all();
    if (timerHandlerThread_.joinable())
    {
        timerHandlerThread_.join();
    }
}

Timer TimerMgr::createTimer(long long timeoutMicros, TimerCallback callback, bool periodic)
{
    int id = timerId_.fetch_add(1);
    std::cout << "Create: Timer id " << id << " created with timeout " << timeoutMicros << " microseconds and periodic " << periodic << std::endl;
    return Timer(id, timeoutMicros, std::move(callback), periodic);
}

void TimerMgr::enqueueNode(const TimerNodeRefPtr& node)
{
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(node);
    nodeMap_[node->id_] = node;
    std::cout << "Enqueue: Timer id " << node->id_ << " enqueued with timeout " << node->timeout_ << std::endl;
    cond_.notify_one();
}

TimerMgr::TimerNodeRefPtr TimerMgr::next()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return !queue_.empty() || !mainThreadAlive; });

    if (!mainThreadAlive)
    {
        return nullptr;
    }

    while (!queue_.empty())
    {
        auto node = queue_.top();
        queue_.pop();
        if (node->valid_)
        {
            return node;
        }
    }
    return nullptr;
}

void TimerMgr::removeTimer(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodeMap_.find(id);
    if (it != nodeMap_.end())
    {
        it->second->valid_ = false;
        nodeMap_.erase(it);
    }
}

void TimerMgr::processTimerHandler()
{
    std::cout << "processTimerHandler: Thread started." << std::endl;
    while (mainThreadAlive)
    {
        auto node = next();
        if (!node)
        {
            continue;
        }

        auto now = currentTimeMicros();
        if (node->when_ <= now)
        {
            std::cout << "Handler: Executing callback for timer id " << node->id_ << std::endl;
            node->callback_();

            if (node->periodic_ && node->valid_)
            {
                node->when_ = now + node->timeout_;
                enqueueNode(node);
            }
            else
            {
                node->valid_ = false;
                removeTimer(node->id_);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::microseconds(node->when_ - now));
            enqueueNode(node);
        }
    }
    std::cout << "processTimerHandler: Thread ended." << std::endl;
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
    auto timerNode = timer.getTimerNode();
    long long currentTime = currentTimeMicros();
    timer.start(currentTime);
    std::cout << "Start: Timer id " << timerNode->id_ << " started at " << timerNode->when_ << std::endl;
    enqueueNode(timerNode);
    return true;
}

void TimerMgr::stop(Timer& timer)
{
    auto timerNode = timer.getTimerNode();
    if (timerNode)
    {
        timerNode->valid_ = false;
        removeTimer(timerNode->id_);
        std::cout << "Stop: Timer id " << timerNode->id_ << " stopped" << std::endl;
        cond_.notify_one();
    }
}

size_t TimerMgr::getActiveTimerCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return nodeMap_.size();
}

} // namespace Nazl
