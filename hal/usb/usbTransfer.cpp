//
// Created by zwz on 2024/11/3.
//

#include "usbTransfer.h"
TransferResult InterruptTransferStrategy::transfer(libusb_device_handle* handle,
        uint8_t endpoint,
        uint8_t* data,
        size_t size,
        unsigned int timeout)
{
    if (!handle || !data)
    {
        return TransferResult{false, 0, -1, "Invalid parameters"};
    }

    int transferred = 0;
    int result = libusb_interrupt_transfer(handle,
                                           endpoint,
                                           data,
                                           static_cast<int>(size),
                                           &transferred,
                                           timeout);

    if (result == 0)
    {
        return TransferResult{true, transferred, 0, "Success"};
    }
    else
    {
        return TransferResult{false, transferred, result, libusb_error_name(result)};
    }
}

TransferResult BulkTransferStrategy::transfer(libusb_device_handle* handle,
        uint8_t endpoint,
        uint8_t* data,
        size_t size,
        unsigned int timeout)
{
    if (!handle || !data)
    {
        return TransferResult{false, 0, -1, "Invalid parameters"};
    }

    int transferred = 0;
    int result = libusb_bulk_transfer(handle,
                                      endpoint,
                                      data,
                                      static_cast<int>(size),
                                      &transferred,
                                      timeout);

    if (result == 0)
    {
        return TransferResult{true, transferred, 0, "Success"};
    }
    else
    {
        return TransferResult{false, transferred, result, libusb_error_name(result)};
    }
}

TransferResult AsyncTransferStrategy::transfer(libusb_device_handle* handle,
        uint8_t endpoint,
        uint8_t* data,
        size_t size,
        unsigned int timeout)
{
    if (!handle || !data)
    {
        return TransferResult{false, 0, -1, "Invalid parameters"};
    }

    libusb_transfer* transfer = libusb_alloc_transfer(0);
    if (!transfer)
    {
        return TransferResult{false, 0, -1, "Failed to allocate transfer"};
    }

    libusb_fill_bulk_transfer(transfer,
                              handle,
                              endpoint,
                              data,
                              static_cast<int>(size),
                              transferCallback,
                              this,
                              timeout);

    int result = libusb_submit_transfer(transfer);
    if (result < 0)
    {
        libusb_free_transfer(transfer);
        return TransferResult{false, 0, result, libusb_error_name(result)};
    }

    return TransferResult{true, 0, 0, "Transfer submitted"};
}

void LIBUSB_CALL AsyncTransferStrategy::transferCallback(libusb_transfer* transfer)
{
    auto* strategy = static_cast<AsyncTransferStrategy*>(transfer->user_data);
    TransferResult result;
    result.success = (transfer->status == LIBUSB_TRANSFER_COMPLETED);
    result.transferredBytes = transfer->actual_length;
    result.errorCode = transfer->status;
    if (!result.success)
    {
        result.errorMessage = libusb_error_name(result.errorCode);
    }

    if (strategy->callback_)
    {
        strategy->callback_(result);
    }

    libusb_free_transfer(transfer);
}
TransferResult UsbTransferCommand::execute()
{
    if (!device_ || !strategy_)
    {
        return TransferResult{false, 0, -1, "Invalid device or strategy"};
    }

    auto* handle = device_->getHandle();
    if (!handle)
    {
        return TransferResult{false, 0, -1, "Device not opened"};
    }

    std::vector<uint8_t> dataCopy = data_;
    return strategy_->transfer(handle,
                               endpoint_,
                               dataCopy.data(),
                               dataCopy.size(),
                               timeout_);
}
TransferResult UsbTransferManager::bulkTransfer(
    uint8_t endpoint,
    const std::vector<uint8_t>& data,
    unsigned int timeout)
{
    auto strategy = std::make_shared<BulkTransferStrategy>();
    UsbTransferCommand cmd(
        device_,
        endpoint,
        data,
        timeout,
        strategy);

    return cmd.execute();
}

TransferResult UsbTransferManager::interruptTransfer(
    uint8_t endpoint,
    const std::vector<uint8_t>& data,
    unsigned int timeout)
{

    auto strategy = std::make_shared<InterruptTransferStrategy>();
    UsbTransferCommand cmd(
        device_,
        endpoint,
        data,
        timeout,
        strategy);

    return cmd.execute();
}
void UsbTransferManager::asyncBulkTransfer(uint8_t endpoint,
        const std::vector<uint8_t>& data,
        AsyncTransferStrategy::CompletionCallback callback,
        unsigned int timeout )
{
    auto wrappedCallback = [this, endpoint, callback]
                           (const TransferResult & result)
    {
        if (callback)
        {
            callback(result);
        }

        auto it = activeTransfers_.find(endpoint);
        if (it != activeTransfers_.end())
        {
            activeTransfers_.erase(it);
        }
    };
    auto strategy = std::make_shared<AsyncTransferStrategy>(wrappedCallback);
    UsbTransferCommand cmd(device_,
                           endpoint,
                           data,
                           timeout,
                           strategy);

    auto result = cmd.execute();
    if (result.success)
    {
        auto it = activeTransfers_.find(endpoint);
        if (it != activeTransfers_.end())
        {
            libusb_cancel_transfer(it->second);
        }
    }
}
void UsbTransferManager::cancelTransfer(uint8_t endpoint)
{
    auto it = activeTransfers_.find(endpoint);
    if (it != activeTransfers_.end())
    {
        libusb_cancel_transfer(it->second);
    }
}