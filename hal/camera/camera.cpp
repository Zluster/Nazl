//
// Created by zwz on 2024/11/10.
//

#include "camera.h"
#include "v4l2Camera.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>
#include <string.h>
#include <iostream>

V4L2Camera::V4L2Camera(const std::string& deviceName)
    : deviceName_(deviceName)
    , fd_(-1)
    , width_(640)
    , height_(480)
    , pixelFormat_(V4L2_PIX_FMT_YUYV)
    , fps_(30)
    , isStreaming_(false)
    , running_(false)
{
}
V4L2Camera::~V4L2Camera()
{
    if (isStreaming_)
    {
        stop();
    }
    if (fd_ != -1)
    {
        close();
    }
}
bool V4L2Camera::waitForFrame(int timeout_ms)
{
    pollfd fds =
    {
        .fd = fd_,
        .events = POLLIN,
        .revents = 0
    };

    int ret = poll(&fds, 1, timeout_ms);
    if (ret < 0)
    {
        std::cerr << "poll failed: " << strerror(errno) << std::endl;
        return false;
    }
    if (ret == 0)
    {
        return false;
    }
    return (fds.revents & POLLIN) != 0;
}
bool V4L2Camera::open()
{
    fd_ = open(deviceName_.c_str(), O_RDWR);
    if (fd_ == -1)
    {
        std::cerr << "open failed: " << strerror(errno) << std::endl;
        return false;
    }
    v4l2_capability cap;
    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0)
    {
        std::cerr << "Failed to query capabilities: "
                  << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        std::cerr << "Device does not support video capture" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        std::cerr << "Device does not support streaming I/O" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    isp_ = std::make_shared<ISP>(fd_);

    return true;
}
bool V4L2Camera::close()
{
    if (fd_ != -1)
    {
        close(fd_);
        fd_ = -1;
    }
    return true;
}
bool V4L2Camera::setFormat(int width, int height, int pixelFormat)
{
    if (fd_ == -1)
    {
        return false;
    }
    v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelFormat;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0)
    {
        std::cerr << "VIDIOC_S_FMT failed: " << strerror(errno) << std::endl;
        return false;
    }
    width_ = fmt.fmt.pix.width;
    height_ = fmt.fmt.pix.height;
    pixelFormat_ = fmt.fmt.pix.pixelformat;
    return true;
}
bool V4L2Camera::setFrameRate(int fps)
{
    if (fd_ < 0)
    {
        return false;
    }
    v4l2_streamparm parm = {};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;

    if (ioctl(fd_, VIDIOC_S_PARM, &parm) < 0)
    {
        std::cerr << "Failed to set frame rate: " << strerror(errno) << std::endl;
        return false;
    }

    fps_ = parm.parm.capture.timeperframe.denominator /
           parm.parm.capture.timeperframe.numerator;

    return true;
}
bool V4L2Camera::initMmap()
{
    v4l2_requestbuffers req = {};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0)
    {
        std::cerr << "Failed to request buffers: " << strerror(errno) << std::endl;
        return false;
    }

    buffers_.resize(req.count);

    for (size_t i = 0; i < req.count; ++i)
    {
        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0)
        {
            std::cerr << "Failed to query buffer: " << strerror(errno) << std::endl;
            return false;
        }

        buffers_[i].length = buf.length;
        buffers_[i].start = mmap(nullptr, buf.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 fd_, buf.m.offset);

        if (buffers_[i].start == MAP_FAILED)
        {
            std::cerr << "Failed to mmap buffer: " << strerror(errno) << std::endl;
            return false;
        }
    }

    return true;
}

bool V4L2Camera::queueBuffers()
{
    for (size_t i = 0; i < buffers_.size(); ++i)
    {
        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
        {
            std::cerr << "Failed to queue buffer: " << strerror(errno) << std::endl;
            return false;
        }
    }
    return true;
}

bool V4L2Camera::start()
{
    if (fd_ < 0 || isStreaming_)
    {
        return false;
    }
    if (!initMmap())
    {
        return false;
    }
    if (!queueBuffers())
    {
        return false;
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0)
    {
        std::cerr << "VIDIOC_STREAMON failed: " << strerror(errno) << std::endl;
        return false;
    }
    isStreaming_ = true;
    running_ = true;
    captureThread_ = std::thread(&V4L2Camera::captureThread, this);
    return true;
}

void V4L2Camera::stop()
{
    if (!isStreaming_)
    {
        return;
    }

    running_ = false;
    if (captureThread_.joinable())
    {
        captureThread_.join();
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0)
    {
        std::cerr << "Failed to stop streaming: " << strerror(errno) << std::endl;
    }
    for (auto& buffer : buffers_)
    {
        if (buffer.start != nullptr && buffer.start != MAP_FAILED)
        {
            munmap(buffer.start, buffer.length);
        }
    }
    isStreaming_ = false;
}
bool V4L2Camera::waitForFrame(int timeout_ms)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int r = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (r < 0)
    {
        std::cerr << "Select error: " << strerror(errno) << std::endl;
        return false;
    }
    if (r == 0)
    {
        std::cerr << "Select timeout" << std::endl;
        return false;
    }
    return true;
}
void V4L2Camera::captureThread()
{
    while (running_)
    {
        if (!waitForFrame(1000))
        {
            continue;
        }

        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
        {
            std::cerr << "VIDIOC_DQBUF failed: " << strerror(errno) << std::endl;
            continue;
        }

        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
        {
            std::cerr << "VIDIOC_QBUF failed: " << strerror(errno) << std::endl;
        }
    }
}
