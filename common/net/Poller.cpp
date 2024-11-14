//
// Created by zwz on 2024/11/12.
//

#include "Poller.h"
#include "Channel.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
Poller::Poller(EventLoop *loop) : ownerLoop_(loop), epollFd_(epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollFd_ < 0)
    {
        exit(1);
    }
}
Poller::~Poller()
{
    ::close(epollFd_);
}
void Poller::poll(int timeoutMs, ChannelList *activeChannels)
{
    int numEvents = ::epoll_wait(epollFd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    if ( numEvents > 0)
    {
        fillActiveChannels(numEvents, activeChannels);
        if (static_cast<size_t> (numEvents) == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if ( numEvents == 0)
    {
        // nothing happend
    }
    else
    {
        if (errno != EINTR)
        {
            exit(1);
        }
    }
}

void Poller::fillActiveChannels(int numEvents, Poller::ChannelList *activeChannels)
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}
void Poller::updateChannel(Channel *channel)
{
    const int fd = channel->fd();
    if (channel->index() < 0)
    {
        struct epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = channel->events();
        event.data.ptr = channel;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0)
        {
            std::cerr << "epoll_ctl error" << std::endl;
        }
        channels_[fd] = channel;
    }
    else
    {
        struct epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = channel->events();
        event.data.ptr = channel;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0)
        {
            std::cerr << "epoll_ctl error" << std::endl;
        }
    }
}
void Poller::removeChannel(Channel *channel)
{
    const int fd = channel->fd();
    if (::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) < 0)
    {
        std::cerr << "epoll_ctl error" << std::endl;
    }
    channels_.erase(fd);
}