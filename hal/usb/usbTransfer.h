//
// Created by zwz on 2024/11/3.
//

#ifndef NAZL_HAL_USB_USBTRANSFER_H_
#define NAZL_HAL_USB_USBTRANSFER_H_
#include <map>
#include "usbDevice.h"
class TransferResult
{
public:
    bool success;
    int transferredBytes;
    int errorCode;
    std::string errorMessage;

    TransferResult(bool success = false, int bytes = 0, int code = 0,
                   const std::string& message = "")
        : success(success)
        , transferredBytes(bytes)
        , errorCode(code)
        , errorMessage(message) {}
};

class UsbTransferStrategy
{
public:
    virtual ~UsbTransferStrategy() = default;

    virtual TransferResult transfer(libusb_device_handle* handle,
                                    uint8_t endpoint,
                                    uint8_t* data,
                                    size_t size,
                                    unsigned int timeout) = 0;
};

class InterruptTransferStrategy : public UsbTransferStrategy
{
public:
    TransferResult transfer(libusb_device_handle* handle,
                            uint8_t endpoint,
                            uint8_t* data,
                            size_t size,
                            unsigned int timeout) override;
};

class BulkTransferStrategy : public UsbTransferStrategy
{
public:
    TransferResult transfer(libusb_device_handle* handle,
                            uint8_t endpoint,
                            uint8_t* data,
                            size_t size,
                            unsigned int timeout) override;
};

class AsyncTransferStrategy : public UsbTransferStrategy
{
public:
    using CompletionCallback = std::function<void(const TransferResult&)>;

    explicit AsyncTransferStrategy(CompletionCallback callback)
        : callback_(callback) {}

    TransferResult transfer(libusb_device_handle* handle,
                            uint8_t endpoint,
                            uint8_t* data,
                            size_t size,
                            unsigned int timeout) override;

private:
    CompletionCallback callback_;
    static void LIBUSB_CALL transferCallback(libusb_transfer* transfer);
};
class UsbTransferCommand
{
public:
    UsbTransferCommand(std::shared_ptr<UsbDevice> device,
                       uint8_t endpoint,
                       const std::vector<uint8_t>& data,
                       unsigned int timeout,
                       std::shared_ptr<UsbTransferStrategy> strategy)
        : device_(device)
        , endpoint_(endpoint)
        , data_(data)
        , timeout_(timeout)
        , strategy_(strategy)
    {}

    TransferResult execute();

private:
    std::shared_ptr<UsbDevice> device_;
    uint8_t endpoint_;
    std::vector<uint8_t> data_;
    unsigned int timeout_;
    std::shared_ptr<UsbTransferStrategy> strategy_;
};

class UsbTransferManager
{
public:
    explicit UsbTransferManager(std::shared_ptr<UsbDevice> device)
        : device_(device)
    {
        if (!device_)
        {
            throw std::invalid_argument("Device cannot be null");
        }
    }
    ~UsbTransferManager()
    {
        for (auto& [endpoint, transfer] : activeTransfers_)
        {
            if (transfer)
            {
                libusb_cancel_transfer(transfer);
            }
        }
    }
    TransferResult bulkTransfer(uint8_t endpoint, const std::vector<uint8_t>& data, unsigned int timeout = 1000);
    TransferResult interruptTransfer(uint8_t endpoint, const std::vector<uint8_t>& data, unsigned int timeout = 1000);
    void asyncBulkTransfer(uint8_t endpoint,
                           const std::vector<uint8_t>& data,
                           AsyncTransferStrategy::CompletionCallback callback,
                           unsigned int timeout = 1000);
    void cancelTransfer(uint8_t endpoint);
private:
    std::shared_ptr<UsbDevice> device_;
    std::map<uint8_t, libusb_transfer*> activeTransfers_;
};
#endif //NAZL_HAL_USB_USBTRANSFER_H_
