//
// Created by zwz on 2024/11/3.
//

#ifndef NAZL_HAL_USB_USBDEVICE_H_
#define NAZL_HAL_USB_USBDEVICE_H_
#include <libusb-1.0/libusb.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

class UsbContext
{
public:
    static UsbContext& getInstance()
    {
        static UsbContext instance;
        return instance;
    }

    libusb_context* getContext() const
    {
        return context_;
    }

    void setDebugLevel(int level)
    {
        libusb_set_option(context_, LIBUSB_OPTION_LOG_LEVEL, level);
    }

private:
    UsbContext() : context_(nullptr)
    {
        int result = libusb_init(&context_);
        if (result != LIBUSB_SUCCESS)
        {
            throw std::runtime_error("Failed to initialize libusb");
        }
    }

    ~UsbContext()
    {
        if (context_)
        {
            libusb_exit(context_);
        }
    }

    UsbContext(const UsbContext&) = delete;
    UsbContext& operator=(const UsbContext&) = delete;

    libusb_context* context_;
};

class UsbDevice
{
public:
    struct DeviceInfo
    {
        uint16_t vendorId;
        uint16_t productId;
        uint8_t busNumber;
        uint8_t deviceAddress;
        int speed;
        std::string manufacturer;
        std::string product;
        std::string serialNumber;
        uint8_t configurationValue;
        std::vector<uint8_t> interfaces;
    };

    explicit UsbDevice(libusb_device* device);
    virtual ~UsbDevice();

    virtual bool open();
    virtual void close();
    virtual bool isOpened() const
    {
        return handle_ != nullptr;
    }

    // 新增功能
    bool claimInterface(int interface);
    bool releaseInterface(int interface);
    bool setConfiguration(int configuration);
    bool resetDevice();
    bool kernelDriverActive(int interface);
    bool detachKernelDriver(int interface);
    bool attachKernelDriver(int interface);

    // 获取设备信息
    const DeviceInfo& getInfo() const
    {
        return info_;
    }
    bool getStringDescriptor(uint8_t index, std::string& data);
    bool getDeviceDescriptor(libusb_device_descriptor& desc);
    bool getConfigDescriptor(uint8_t config_index, libusb_config_descriptor** config);

    // 控制传输
    int controlTransfer(uint8_t bmRequestType,
                        uint8_t bRequest,
                        uint16_t wValue,
                        uint16_t wIndex,
                        unsigned char* data,
                        uint16_t wLength,
                        unsigned int timeout);

    libusb_device_handle* getHandle() const
    {
        return handle_;
    }

protected:
    bool readDeviceInfo();

    libusb_device* device_;
    libusb_device_handle* handle_;
    DeviceInfo info_;
    std::vector<bool> claimedInterfaces_;
};
class UsbDeviceFactory
{
public:
    static std::vector<std::shared_ptr<UsbDevice>> enumDevices();
    static std::shared_ptr<UsbDevice> findDevice(uint16_t vendorId, uint16_t productId);
};
class UsbHotPlugMonitor
{
public:
    using DeviceCallback = std::function<void(std::shared_ptr<UsbDevice>)>;
    UsbHotPlugMonitor();
    ~UsbHotPlugMonitor();
    void setArriveCallback(DeviceCallback callback)
    {
        arrivalCallback_ = callback;
    }
    void setRemoveCallback(DeviceCallback callback)
    {
        removalCallback_ = callback;
    }
    void start();
    void stop();
private:
    void monitorThread();
    static int LIBUSB_CALL hotplugCallback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void* user_data);
    DeviceCallback arrivalCallback_;
    DeviceCallback removalCallback_;
    std::atomic<bool> running_;
    std::thread monitorThread_;
    libusb_hotplug_callback_handle callback_handle_;
};
#endif //NAZL_HAL_USB_USBDEVICE_H_
