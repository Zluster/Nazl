//
// Created by zwz on 2024/11/12.
//

#ifndef NAZL_COMMON_NET_POLLER_H_
#define NAZL_COMMON_NET_POLLER_H_
#include <vector>
#include <map>
class Channel;
class EventLoop;
class Poller
{
public:
    typedef std::vector<Channel*> ChannelList;
    Poller(EventLoop* loop);
    ~Poller();
    void poll(int timeoutMs, ChannelList* activeChannels);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
private:
    void fillActiveChannels(int timeoutMs, ChannelList* activeChannels);
    typedef std::vector<struct epoll_event> EventList;
    typedef std::map<int, Channel*> ChannelMap;

    EventLoop* ownerLoop_;
    int epollfd_;
    EventList events_;
    ChannelMap channels_;
};

#endif //NAZL_COMMON_NET_POLLER_H_
