//
// Created by zwz on 2024/11/12.
//

#include "EventLoop.h"
#include "Channel.h"
#include "Poller.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <signal.h>
#include <cassert>

EventLoop::EventLoop() : looping_(false), quit_(false), threadId_(std::this_thread::get_id()), poller_(new Poller(this)), wakeupFd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)), wakeupChannel_(new Channel(this, wakeupFd_), callingPendingFunctors(false))
{
    if ( wakeupFd_ < 0)
    {
        std::cerr << "EventLoop::EventLoop() eventfd error" << std::endl;
        abort();
    }
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->removeChannel();
    ::close(wakeupFd_);
}
void EventLoop::loop()
{
    assert(looping_ == false);
    assert(isInLoopThread())
    looping_ = true;
    quit_ = false;
    while (!quit_)
    {
        activeChannels_.clear();
        poller_->poll(10000, &activeChannels_);
        for (auto& channel : activeChannels_)
        {
            channel->handleEvent();
        }
        doPendingFunctors();
    }
    looping_ = false;
}
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {
        wakeUp();
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    poller_->removeChannel(channel);
}
void EventLoop::runInLoop(const std::function<void()>& cb)
{
    if ( isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(cb);
    }
}
void EventLoop::queueInLoop(const std::function<void()>& cb)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(cb);
    }
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeUp();
    }
}
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one || n != 8)
    {
        LOG_INFO << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<std::function<void()>> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for (const auto& functor : functors)
    {
        functor();
    }
    callingPendingFunctors_ = false;
}