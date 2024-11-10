//
// Created by zwz on 2024/11/3.
//
#include <cstring>
#include "usbDevice.h"

UsbDevice::UsbDevice(libusb_device *device) : device_(device), handle_(nullptr)
{
    if (device_)
    {
        libusb_ref_device(device_);
    }
    memset(&info_, 0, sizeof(info_));
}
UsbDevice::~UsbDevice()
{
    close();
    if (device_)
    {
        libusb_unref_device(device_);
        device_ = nullptr;
    }
}
bool UsbDevice::open()
{
    if (handle_)
    {
        return true;
    }
    int result = libusb_open(device_, &handle_);
    if (result != 0)
    {
        return false;
    }
    if (!readDeviceInfo())
    {
        close();
        return false;
    }
    return true;
}
void UsbDevice::close()
{
    if (handle_)
    {
        for (size_t i = 0; i < claimedInterfaces_.size(); ++i)
        {
            if (claimedInterfaces_[i])
            {
                libusb_release_interface(handle_, i);
            }
        }
        libusb_close(handle_);
        handle_ = nullptr;
    }
}
bool UsbDevice::readDeviceInfo()
{
    if (!handle_)
    {
        return false;
    }
    libusb_device_descriptor desc;
    int result = libusb_get_device_descriptor(device_, &desc);
    if (LIBUSB_SUCCESS != result)
    {
        return false;
    }
    info_.vendorId = desc.idVendor;
    info_.productId = desc.idProduct;
    info_.busNumber = libusb_get_bus_number(device_);
    info_.deviceAddress = libusb_get_device_address(device_);
    info_.speed = libusb_get_device_speed(device_);
    unsigned char string_data[256];
    if (desc.iManufacturer > 0)
    {
        result = libusb_get_string_descriptor_ascii(handle_, desc.iManufacturer, string_data, sizeof(string_data));
        if (result > 0)
        {
            info_.manufacturer = std::string(reinterpret_cast<char*>(string_data), result);
        }
    }
    if (desc.iProduct > 0)
    {
        result = libusb_get_string_descriptor_ascii(handle_, desc.iProduct, string_data, sizeof(string_data));
        if (result > 0)
        {
            info_.product = std::string(reinterpret_cast<char*>(string_data), result);
        }
    }
    if (desc.iSerialNumber > 0)
    {
        result = libusb_get_string_descriptor_ascii(handle_, desc.iSerialNumber, string_data, sizeof(string_data));
        if (result > 0)
        {
            info_.serialNumber = std::string(reinterpret_cast<char*>(string_data), result);
        }
    }
    return true;
}
bool UsbDevice::claimInterface(int interface)
{
    if (!handle_)
    {
        return false;
    }
    int result = libusb_kernel_driver_active(handle_, interface);
    if (result < 0)
    {
        // 处理检查错误
        return false;
    }
    if (result == 1)
    {
        result = libusb_detach_kernel_driver(handle_, interface);
        if (result < 0)
        {
            return false;
        }
    }
    result = libusb_claim_interface(handle_, interface);
    if (result != 0)
    {
        if (result == 1)
        {
            libusb_attach_kernel_driver(handle_, interface);
        }
        return false;
    }
    if (interface >= claimedInterfaces_.size())
    {
        claimedInterfaces_.resize(interface + 1, false);
    }
    claimedInterfaces_[interface] = true;

    return true;
}

bool UsbDevice::releaseInterface(int interface)
{
    if (!handle_ || interface >= claimedInterfaces_.size() || !claimedInterfaces_[interface])
    {
        return false;
    }
    int result = libusb_release_interface(handle_, interface);
    if (result < 0)
    {
        return false;
    }
    result = libusb_kernel_driver_active(handle_, interface);
    if (result == 0)
    {
        result = libusb_attach_kernel_driver(handle_, interface);
        if (result < 0)
        {
            return false;
        }
    }
    claimedInterfaces_[interface] = false;

    return true;
}
int UsbDevice::controlTransfer(uint8_t bmRequestType,
                               uint8_t bRequest,
                               uint16_t wValue,
                               uint16_t wIndex,
                               unsigned char* data,
                               uint16_t wLength,
                               unsigned int timeout)
{
    if (!handle_)
    {
        return LIBUSB_ERROR_NO_DEVICE;
    }

    return libusb_control_transfer(handle_,
                                   bmRequestType,
                                   bRequest,
                                   wValue,
                                   wIndex,
                                   data,
                                   wLength,
                                   timeout);
}

UsbHotPlugMonitor::UsbHotPlugMonitor()
    : running_(false), callback_handle_(0)
{

}
UsbHotPlugMonitor::~UsbHotPlugMonitor()
{
    stop();
}
void UsbHotPlugMonitor::start()
{
    if (running_)
    {
        return;
    }
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
    {
        throw std::runtime_error("libusb does not support hotplug");
    }
    int result = libusb_hotplug_register_callback(
                     UsbContext::getInstance().getContext(),
                     static_cast<libusb_hotplug_event>(
                         LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                         LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
                     LIBUSB_HOTPLUG_ENUMERATE,
                     LIBUSB_HOTPLUG_MATCH_ANY,
                     LIBUSB_HOTPLUG_MATCH_ANY,
                     LIBUSB_HOTPLUG_MATCH_ANY,
                     hotplugCallback,
                     this,
                     &callback_handle_);
    if (result != 0)
    {
        throw std::runtime_error("libusb_hotplug_register_callback failed");
    }
    running_ = true;
    monitorThread_ = std::thread(&UsbHotPlugMonitor::monitorThread, this);
}

void UsbHotPlugMonitor::stop()
{
    if (!running_)
    {
        return;
    }

    running_ = false;

    if (callback_handle_)
    {
        libusb_hotplug_deregister_callback(
            UsbContext::getInstance().getContext(),
            callback_handle_);
    }

    if (monitorThread_.joinable())
    {
        monitorThread_.join();
    }
}
void UsbHotPlugMonitor::monitorThread()
{
    while (running_)
    {
        libusb_handle_events_completed(
            UsbContext::getInstance().getContext(),
            nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
int LIBUSB_CALL UsbHotPlugMonitor::hotplugCallback(
    libusb_context* ctx,
    libusb_device* device,
    libusb_hotplug_event event,
    void* user_data)
{

    auto* monitor = static_cast<UsbHotPlugMonitor*>(user_data);
    auto usbDevice = std::make_shared<UsbDevice>(device);

    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
    {
        if (monitor->arrivalCallback_)
        {
            monitor->arrivalCallback_(usbDevice);
        }
    }
    else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
    {
        if (monitor->removalCallback_)
        {
            monitor->removalCallback_(usbDevice);
        }
    }

    return 0;
}