/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class StorageKvSimulatorImpl;
class StorageFsSimulatorImpl;

class StorageSimulatorDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "StorageSimulator";
    static constexpr const char *KV_IFACE_NAME = "StorageSimulator:KV";
    static constexpr const char *FS_IFACE_NAME = "StorageSimulator:FS";

    StorageSimulatorDevice(const StorageSimulatorDevice &) = delete;
    StorageSimulatorDevice &operator=(const StorageSimulatorDevice &) = delete;
    StorageSimulatorDevice(StorageSimulatorDevice &&) = delete;
    StorageSimulatorDevice &operator=(StorageSimulatorDevice &&) = delete;

    static StorageSimulatorDevice &get_instance()
    {
        static StorageSimulatorDevice instance;
        return instance;
    }

private:
    StorageSimulatorDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~StorageSimulatorDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<StorageKvSimulatorImpl> kv_iface_;
    std::shared_ptr<StorageFsSimulatorImpl> fs_iface_;
};

} // namespace esp_brookesia::hal
