//
// Created by zwz on 2024/9/22.
//

#include "timer.h"
namespace Nazl {
bool Timer::start(long long currentTimeMicros) {
    timerNode_.when_ = currentTimeMicros + timerNode_.timeout_;
    return true;
}
void Timer::stop() {
    timerNode_.valid_ = false;
}
TimerNodeRef TimerNodeQueue::next() {
    MutexType::Lock lock(mutex_);
    if (timerNodeQueue_.empty()) {
        cond_.wait(lock);
    }
    long long currentTimeMicros = TimerMgr::currentTimeMicros();
    auto it = std::find_if(timerNodeQueue_.begin(), timerNodeQueue_.end(),
                   [currentTimeMicros](const TimerNodeRefPtr& node) {
            return node->when_ <= currentTimeMicros;
    });
    if (it != timerNodeQueue_.end()) {
        auto node = *it;
        timerNodeQueue_.erase(it);
        return node;
    }
    return nullptr;
}
bool TimerNodeQueue::enqueueNode(const TimerNodeRefPtr& node) {
    MutexType::Lock lock(mutex_);
    node->when_ = when;
    node->valid_ = true;
    timerNodeQueue_.push_back(node);
    cond_.notify_one();
    return true;
}
void TimerNodeQueue::removeNode(int id) {
    MutexType::Lock lock(mutex_);
    auto it = std::find_if(timerNodeQueue_.begin(), timerNodeQueue_.end(),
                   [id](const TimerNodeRefPtr& node) {
            return node->id_ == id;
    });
    if (it != timerNodeQueue_.end()) {
        timerNodeQueue_.erase(it);
    }
}
bool TimerNodeQueue::hadNode(int id) const {
    MutexType::Lock lock(mutex_);
    auto it = std::find_if(timerNodeQueue_.begin(), timerNodeQueue_.end(),
                   [id](const TimerNodeRefPtr& node) {
            return node->id_ == id && node->valid_;
    });
    return it != timerNodeQueue_.end();
}
bool TimerNodeQueue::hasNode(int id) const {
    MutexType::Lock lock(mutex_);
    return std::any_of(timerNodeQueue_.begin(), timerNodeQueue_.end(),
                       [id](const TimerNodeRefPtr& node) {
                return node->id_ == id && node->valid_;
    });
}
void TimerNodeQueue::waitForItems() {
    MUtexType::Lock lock(mutex_);
    cond_.wait(lock, [this] { return !timerNodeQueue_.empty(); });
}
void TimerNodeQueue::showNode() const {
    MutexType::Lock lock(mutex_);
    for (const auto& node : timerNodeQueue_) {
        std::cout << "id: " << node->id_ << " timeout: " << node->timeout_ << " when: " << node->when_ << std::endl;
    }
}
TimerMgr::TimerMgr() {
    timerTickThread_ = std::thread([this] { this->timerTickThreadFunc(); });};
    timerHandlerThread_ = std::thread([this] { this->timerHandlerThreadFunc(); });
}
TimerMgr::~TimerMgr() {
    mainThreadAlive = false;
    cond_.notify_all();
    if (timerTickThread_.joinable()) {
        timerTickThread_.join();
    }
    if (timerHandlerThread_.joinable()) {
        timerHandlerThread_.join();
    }
}
Timer TimerMgr::createTimer(long long timeoutMicros, TimerCallback callback, bool periodic) {
    int id = timerId.fetch_add(1);
    Timer timer(id, timeoutMicros, callback, periodic);
    return timer;
}
void TimerMgr::processTimerTick() {
    while(mainThreadAlive) {
        auto timerNode = timerNodeQueue_.next();
        if (timerNode) {
            continue;
        }
        if (timerNode->valid_) {
            continue;
        }
        if( timerNode->periodic_) {
            timerNode->when_ = TimerMgr::currentTimeMicros() + timerNode->timeout_;
            timerNodeQueue_.enqueueNode(timerNode);
        } else {
            timerNode->valid_ = false;
        }
        timerNodeList_->push_back(timerNode);
        cond_.notify_one();
    }
}
void TimerMgr::processTimerHandler() {
    while(mainThreadAlive) {
        MutexType::Lock lock(mutex_);
        cond_.wait(lock, [this] { return !timerNodeList_->empty(); });
        auto timerNode = timerNodeList_->pop_front();
        if (!timerNode || !timerNode->valid_)  {
            continue;
        }
        timerNode->callback_(timerNode->id_);

    }
}
long long TimerMgr::currentTimeMicros() {
    auto now = std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now());
    std::chrono::microseconds span =
            std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    return span.count();
}
bool TimerMgr::start(Timer& timer) {
    MutexType::Lock lock(mutex_);
    const auto& timerNode = timer.getTimerNode();
    if (timerNodeQueue_.hadNode(timerNode->id_)) {
        timerNodeQueue_.removeNode(timerNode->id_);
    }
    timerHandlers_.erase(std::remove(timerHandlers_.begin(), timerHandlers_.end(),
            [&timerNode](const TimerNodeRefPtr& node) {
                return node->id_ == timerNode.id_;
            }),
         timerHandlers_.end()));
    timer.start(currentTimeMicros());
    return timerTimeQueue_.enqueueNode(std::make_shared<TimerNode>(timerNode), timerNode->when_);
}
void TimerMgr::stop(Timer& timer) {
    MutexType::Lock lock(mutex_);
    const auto& timerNode = timer.getTimerNode();
    timerHandlers_.erase(std::remove(timerHandlers_.begin(), timerHandlers_.end(),
            [&timerNode](const TimerNodeRefPtr& node) {
                return node->id_ == timerNode.id_;
            }),
         timerHandlers_.end()));
    timer.stop();
}
}