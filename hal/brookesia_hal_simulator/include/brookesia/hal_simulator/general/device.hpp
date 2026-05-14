/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class BoardInfoSimulatorImpl;

class GeneralSimulatorDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "GeneralSimulator";
    static constexpr const char *BOARD_INFO_IFACE_NAME = "GeneralSimulator:BoardInfo";

    GeneralSimulatorDevice(const GeneralSimulatorDevice &) = delete;
    GeneralSimulatorDevice &operator=(const GeneralSimulatorDevice &) = delete;
    GeneralSimulatorDevice(GeneralSimulatorDevice &&) = delete;
    GeneralSimulatorDevice &operator=(GeneralSimulatorDevice &&) = delete;

    static GeneralSimulatorDevice &get_instance()
    {
        static GeneralSimulatorDevice instance;
        return instance;
    }

private:
    GeneralSimulatorDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~GeneralSimulatorDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<BoardInfoSimulatorImpl> board_info_iface_;
};

} // namespace esp_brookesia::hal
