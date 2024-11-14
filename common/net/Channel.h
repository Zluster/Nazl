//
// Created by zwz on 2024/11/11.
//

#ifndef NAZL_COMMON_NET_CHANNEL_H_
#define NAZL_COMMON_NET_CHANNEL_H_

class EventLoop;
class Channel
{
public:
    using EventCallback = std::function<void()>;
    Channel(EventLoop *loop, int fd);
    ~Channel();

    void handleEvent();
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }
    int fd() const
    {
        return fd_;
    }
    int events() const
    {
        return events_;
    }
    void set_revents(int revt)
    {
        revents_ = revt;
    }
    bool isNonEvent() const
    {
        return events_ = kNoneEvent;
    }
private:
    void update();

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;
    const int fd_;
    int events_;
    int revents_;

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback errorCallback_;
    EventCallback closeCallback_;
}
};

#endif //NAZL_COMMON_NET_CHANNEL_H_
