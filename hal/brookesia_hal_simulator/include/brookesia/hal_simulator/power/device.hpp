/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class PowerBatterySimulatorImpl;

class PowerSimulatorDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "PowerSimulator";
    static constexpr const char *BATTERY_IFACE_NAME = "PowerSimulator:Battery";

    PowerSimulatorDevice(const PowerSimulatorDevice &) = delete;
    PowerSimulatorDevice &operator=(const PowerSimulatorDevice &) = delete;
    PowerSimulatorDevice(PowerSimulatorDevice &&) = delete;
    PowerSimulatorDevice &operator=(PowerSimulatorDevice &&) = delete;

    static PowerSimulatorDevice &get_instance()
    {
        static PowerSimulatorDevice instance;
        return instance;
    }

private:
    PowerSimulatorDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~PowerSimulatorDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<PowerBatterySimulatorImpl> battery_iface_;
};

} // namespace esp_brookesia::hal
