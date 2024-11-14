//
// Created by zwz on 2024/11/12.
//

#ifndef NAZL_COMMON_NET_EVENTLOOP_H_
#define NAZL_COMMON_NET_EVENTLOOP_H_
class Channel;
class Poller;
class EventLoop
{
public:
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool isInLoopThread() const
    {
        return threadId_ = std::this_thread::get_id();
    }
    void runInLoop(const std::function<void()>& cb);
private:
    void handleRead();
    void doPendingFunctors();
    typedef std::vector<Channel*> ChannelList;
    bool looping_;
    bool quit_;
    std::thread::id threadId_;
    std::unique_ptr<Poller> poller_;
    ChannelList activeChannels_;
    std::vector<std::function<void()>> pendingFunctors_;
    std::mutex mutex_;
    int wakeupFd_;
    bool callingPendingFunctors_;
    std::unique_ptr<Channel> wakeupChannel_;

};

#endif //NAZL_COMMON_NET_EVENTLOOP_H_
