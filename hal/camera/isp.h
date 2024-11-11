//
// Created by zwz on 2024/11/10.
//

#ifndef NAZL_HAL_CAMERA_ISP_H_
#define NAZL_HAL_CAMERA_ISP_H_
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <vector>
class ISP
{
public:
    // ISP参数结构
    struct ISPParams
    {
        int32_t exposure;
        int32_t gain;
        int32_t white_balance;
        int32_t brightness;
        int32_t contrast;
        int32_t saturation;
        int32_t sharpness;
        int32_t hue;
        int32_t gamma;
        bool auto_exposure;
        bool auto_white_balance;
        int32_t exposure_priority;
        int32_t focus;
        bool auto_focus;
        int32_t zoom;
        int32_t backlight_compensation;
        int32_t power_line_frequency;
        int32_t hdr;
    };

    explicit ISP(int fd);

    std::vector<v4l2_queryctrl> getSupportedControls();

    bool setParameters(const ISPParams& params);
    bool getParameters(ISPParams& params);
    bool autoAdjust();
    enum class SceneMode
    {
        AUTO,
        NIGHT,
        SPORTS,
        PORTRAIT,
        LANDSCAPE
    };
    bool setSceneMode(SceneMode mode);
    bool setExposure(int32_t value);
    bool setGain(int32_t value);
    bool setWhiteBalance(int32_t value);
    bool setBrightness(int32_t value);
    bool setContrast(int32_t value);
    bool setSaturation(int32_t value);
    bool setSharpness(int32_t value);
    bool setHue(int32_t value);
    bool setGamma(int32_t value);
    bool setAutoExposure(bool enable);
    bool setAutoWhiteBalance(bool enable);
    bool setExposurePriority(int32_t value);
    bool setFocus(int32_t value);
    bool setAutoFocus(bool enable);
    bool setZoom(int32_t value);
    bool setBacklightCompensation(int32_t value);
    bool setPowerLineFrequency(int32_t value);
    bool setHDR(int32_t value);

private:
    bool setControl(uint32_t id, int32_t value);
    bool getControl(uint32_t id, int32_t& value);
    int fd_;
    std::map<uint32_t, v4l2_queryctrl> supportedControls_;
};
#endif //NAZL_HAL_CAMERA_ISP_H_
