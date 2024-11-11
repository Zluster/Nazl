//
// Created by zwz on 2024/11/10.
//

#include "isp.h"
#include <fstream>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <iostream>

ISP::ISP(int fd) : fd_(fd)
{
    v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof(queryctrl));
    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (ioctl(fd_, VIDIOC_QUERYCTRL, &queryctrl) == 0)
    {
        if (!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED))
        {
            supportedControls_[queryctrl.id] = queryctrl;
        }
        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
}

bool ISP::setControl(uint32_t id, int32_t value)
{
    auto it = supportedControls_.find(id);
    if (it == supportedControls_.end())
    {
        std::cerr << "Control " << id << " not supported" << std::endl;
        return false;
    }

    const auto& ctrl = it->second;
    if (value < ctrl.minimum || value > ctrl.maximum)
    {
        std::cerr << "Value " << value << " out of range ["
                  << ctrl.minimum << ", " << ctrl.maximum << "]" << std::endl;
        return false;
    }

    v4l2_control control = {};
    control.id = id;
    control.value = value;

    if (ioctl(fd_, VIDIOC_S_CTRL, &control) < 0)
    {
        std::cerr << "Failed to set control " << id << ": "
                  << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool ISP::getControl(uint32_t id, int32_t& value)
{
    if (supportedControls_.find(id) == supportedControls_.end())
    {
        std::cerr << "Control " << id << " not supported" << std::endl;
        return false;
    }

    v4l2_control control = {};
    control.id = id;

    if (ioctl(fd_, VIDIOC_G_CTRL, &control) < 0)
    {
        std::cerr << "Failed to get control " << id << ": "
                  << strerror(errno) << std::endl;
        return false;
    }
    value = control.value;
    return true;
}

bool ISP::setParameters(const ISPParams& params)
{
    bool success = true;
    success &= setExposure(params.exposure);
    success &= setGain(params.gain);
    success &= setWhiteBalance(params.white_balance);
    success &= setBrightness(params.brightness);
    success &= setContrast(params.contrast);
    success &= setSaturation(params.saturation);
    success &= setSharpness(params.sharpness);
    success &= setHue(params.hue);
    success &= setGamma(params.gamma);
    success &= setAutoExposure(params.auto_exposure);
    success &= setAutoWhiteBalance(params.auto_white_balance);
    success &= setExposurePriority(params.exposure_priority);
    success &= setFocus(params.focus);
    success &= setAutoFocus(params.auto_focus);
    success &= setZoom(params.zoom);
    success &= setBacklightCompensation(params.backlight_compensation);
    success &= setPowerLineFrequency(params.power_line_frequency);
    success &= setHDR(params.hdr);
    return success;
}

bool ISP::getParameters(ISPParams& params)
{
    bool success = true;
    success &= getControl(V4L2_CID_EXPOSURE, params.exposure);
    success &= getControl(V4L2_CID_GAIN, params.gain);
    success &= getControl(V4L2_CID_WHITE_BALANCE_TEMPERATURE, params.white_balance);
    success &= getControl(V4L2_CID_BRIGHTNESS, params.brightness);
    success &= getControl(V4L2_CID_CONTRAST, params.contrast);
    success &= getControl(V4L2_CID_SATURATION, params.saturation);
    success &= getControl(V4L2_CID_SHARPNESS, params.sharpness);
    success &= getControl(V4L2_CID_HUE, params.hue);
    success &= getControl(V4L2_CID_GAMMA, params.gamma);

    int32_t value;
    success &= getControl(V4L2_CID_EXPOSURE_AUTO, value);
    params.auto_exposure = (value == V4L2_EXPOSURE_AUTO);

    success &= getControl(V4L2_CID_AUTO_WHITE_BALANCE, value);
    params.auto_white_balance = (value == 1);

    success &= getControl(V4L2_CID_EXPOSURE_PRIORITY, params.exposure_priority);
    success &= getControl(V4L2_CID_FOCUS_ABSOLUTE, params.focus);

    success &= getControl(V4L2_CID_FOCUS_AUTO, value);
    params.auto_focus = (value == 1);

    success &= getControl(V4L2_CID_ZOOM_ABSOLUTE, params.zoom);
    success &= getControl(V4L2_CID_BACKLIGHT_COMPENSATION, params.backlight_compensation);
    success &= getControl(V4L2_CID_POWER_LINE_FREQUENCY, params.power_line_frequency);
    success &= getControl(V4L2_CID_WIDE_DYNAMIC_RANGE, params.hdr);

    return success;
}


bool ISP::setSceneMode(SceneMode mode)
{
    ISPParams params;
    switch (mode)
    {
    case SceneMode::NIGHT:
        params.exposure = 1000;
        params.gain = 800;
        params.brightness = 30;
        params.contrast = 128;
        params.auto_exposure = false;
        break;

    case SceneMode::SPORTS:
        params.exposure = 100;
        params.gain = 100;
        params.sharpness = 200;
        params.auto_exposure = false;
        break;

    case SceneMode::PORTRAIT:
        params.exposure = 500;
        params.gain = 200;
        params.saturation = 150;
        params.sharpness = 80;
        break;

    case SceneMode::LANDSCAPE:
        params.exposure = 400;
        params.gain = 150;
        params.saturation = 170;
        params.sharpness = 200;
        params.contrast = 150;
        break;

    case SceneMode::AUTO:
    default:
        params.auto_exposure = true;
        params.auto_white_balance = true;
        params.brightness = 128;
        params.contrast = 128;
        params.saturation = 128;
        params.sharpness = 128;
        break;
    }

    return setParameters(params);
}

// 各个控制参数的具体实现
bool ISP::setExposure(int32_t value)
{
    return setControl(V4L2_CID_EXPOSURE, value);
}

bool ISP::setGain(int32_t value)
{
    return setControl(V4L2_CID_GAIN, value);
}

bool ISP::setWhiteBalance(int32_t value)
{
    return setControl(V4L2_CID_WHITE_BALANCE_TEMPERATURE, value);
}

bool ISP::setBrightness(int32_t value)
{
    return setControl(V4L2_CID_BRIGHTNESS, value);
}

bool ISP::setContrast(int32_t value)
{
    return setControl(V4L2_CID_CONTRAST, value);
}

bool ISP::setSaturation(int32_t value)
{
    return setControl(V4L2_CID_SATURATION, value);
}

bool ISP::setSharpness(int32_t value)
{
    return setControl(V4L2_CID_SHARPNESS, value);
}

bool ISP::setHue(int32_t value)
{
    return setControl(V4L2_CID_HUE, value);
}

bool ISP::setGamma(int32_t value)
{
    return setControl(V4L2_CID_GAMMA, value);
}

bool ISP::setAutoExposure(bool enable)
{
    return setControl(V4L2_CID_EXPOSURE_AUTO,
                      enable ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL);
}

bool ISP::setAutoWhiteBalance(bool enable)
{
    return setControl(V4L2_CID_AUTO_WHITE_BALANCE, enable ? 1 : 0);
}

bool ISP::setExposurePriority(int32_t value)
{
    return setControl(V4L2_CID_EXPOSURE_PRIORITY, value);
}

bool ISP::setFocus(int32_t value)
{
    return setControl(V4L2_CID_FOCUS_ABSOLUTE, value);
}

bool ISP::setAutoFocus(bool enable)
{
    return setControl(V4L2_CID_FOCUS_AUTO, enable ? 1 : 0);
}

bool ISP::setZoom(int32_t value)
{
    return setControl(V4L2_CID_ZOOM_ABSOLUTE, value);
}

bool ISP::setBacklightCompensation(int32_t value)
{
    return setControl(V4L2_CID_BACKLIGHT_COMPENSATION, value);
}

bool ISP::setPowerLineFrequency(int32_t value)
{
    return setControl(V4L2_CID_POWER_LINE_FREQUENCY, value);
}

bool ISP::setHDR(int32_t value)
{
    return setControl(V4L2_CID_WIDE_DYNAMIC_RANGE, value);
}