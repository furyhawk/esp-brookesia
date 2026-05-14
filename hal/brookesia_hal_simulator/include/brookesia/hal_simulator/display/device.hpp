/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class DisplayPanelSimulatorImpl;
class DisplayTouchSimulatorImpl;
class DisplayBacklightSimulatorImpl;

class DisplaySimulatorDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "DisplaySimulator";
    static constexpr const char *PANEL_IFACE_NAME = "DisplaySimulator:Panel";
    static constexpr const char *TOUCH_IFACE_NAME = "DisplaySimulator:Touch";
    static constexpr const char *BACKLIGHT_IFACE_NAME = "DisplaySimulator:Backlight";

    DisplaySimulatorDevice(const DisplaySimulatorDevice &) = delete;
    DisplaySimulatorDevice &operator=(const DisplaySimulatorDevice &) = delete;
    DisplaySimulatorDevice(DisplaySimulatorDevice &&) = delete;
    DisplaySimulatorDevice &operator=(DisplaySimulatorDevice &&) = delete;

    static DisplaySimulatorDevice &get_instance()
    {
        static DisplaySimulatorDevice instance;
        return instance;
    }

private:
    DisplaySimulatorDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~DisplaySimulatorDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<DisplayPanelSimulatorImpl> panel_iface_;
    std::shared_ptr<DisplayTouchSimulatorImpl> touch_iface_;
    std::shared_ptr<DisplayBacklightSimulatorImpl> backlight_iface_;
};

} // namespace esp_brookesia::hal
