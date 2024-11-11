//
// Created by zwz on 2024/11/10.
//

#ifndef NAZL_HAL_CAMERA_CAMERA_H_
#define NAZL_HAL_CAMERA_CAMERA_H_

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <linux/videodev2.h>
#include "isp.h"

class V4L2Camera
{
public:
    struct Buffer
    {
        void* start;
        size_t length;
    };

    explicit V4L2Camera(const std::string& deviceName);
    ~V4L2Camera();

    bool open();
    void close();
    bool start();
    void stop();

    std::shared_ptr<ISP> getISP()
    {
        return isp_;
    }

    bool setFormat(uint32_t width, uint32_t height, uint32_t pixelFormat);
    bool setFrameRate(uint32_t fps);

private:
    bool initDevice();
    bool initMmap();
    bool queueBuffers();
    void captureThread();
    bool waitForFrame(int timeout_ms);

    std::string deviceName_;
    int fd_;
    std::vector<Buffer> buffers_;
    uint32_t width_;
    uint32_t height_;
    uint32_t pixelFormat_;
    uint32_t fps_;
    bool isStreaming_;

    std::shared_ptr<ISP> isp_;
    std::unique_ptr<SharedMemory> sharedMem_;
    std::unique_ptr<FrameBuffer> frameBuffer_;

    std::atomic<bool> running_;
    std::thread captureThread_;
};

#endif //NAZL_HAL_CAMERA_CAMERA_H_
