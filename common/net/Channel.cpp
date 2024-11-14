//
// Created by zwz on 2024/11/11.
//

#include <sys/epoll.h>
#include <poll.h>
#include "Channel.h"
#include "EventLoop.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0) {}
Channel::~Channel()
{
    if (loop_->isInLoopThread())
    {
        assert(!loop_->hasChannel(this));
    }
}
void Channel::update()
{
    loop_->updateChannel(this);
}
void Channel::handleEvent()
{
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }

    }
    if (revents_ & (EPOLLERR | EPOLLHUP))
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    if (revents_ & (EPOLLIN | EPOLLPRI ))
    {
        if (readCallback_)
        {
            readCallback_();
        }
    }
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}