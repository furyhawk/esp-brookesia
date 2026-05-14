/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class WifiEspBackend;

class WifiDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "WiFi";
    static constexpr const char *BASIC_IFACE_NAME = "WiFi:Basic";
    static constexpr const char *STA_IFACE_NAME = "WiFi:STA";
    static constexpr const char *SOFTAP_IFACE_NAME = "WiFi:SoftAP";

    WifiDevice(const WifiDevice &) = delete;
    WifiDevice &operator=(const WifiDevice &) = delete;
    WifiDevice(WifiDevice &&) = delete;
    WifiDevice &operator=(WifiDevice &&) = delete;

    static WifiDevice &get_instance()
    {
        static WifiDevice instance;
        return instance;
    }

private:
    WifiDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~WifiDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<WifiEspBackend> backend_;
};

} // namespace esp_brookesia::hal
