/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class WifiSimulatorBackend;

class WifiSimulatorDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "WiFiSimulator";
    static constexpr const char *BASIC_IFACE_NAME = "WiFiSimulator:Basic";
    static constexpr const char *STA_IFACE_NAME = "WiFiSimulator:STA";
    static constexpr const char *SOFTAP_IFACE_NAME = "WiFiSimulator:SoftAP";

    WifiSimulatorDevice(const WifiSimulatorDevice &) = delete;
    WifiSimulatorDevice &operator=(const WifiSimulatorDevice &) = delete;
    WifiSimulatorDevice(WifiSimulatorDevice &&) = delete;
    WifiSimulatorDevice &operator=(WifiSimulatorDevice &&) = delete;

    static WifiSimulatorDevice &get_instance()
    {
        static WifiSimulatorDevice instance;
        return instance;
    }

private:
    WifiSimulatorDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~WifiSimulatorDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<WifiSimulatorBackend> backend_;
};

} // namespace esp_brookesia::hal
