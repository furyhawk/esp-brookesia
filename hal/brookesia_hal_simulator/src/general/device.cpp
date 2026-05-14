/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_simulator/macro_configs.h"
#if !BROOKESIA_HAL_SIMULATOR_GENERAL_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/general/board_info.hpp"
#include "brookesia/hal_simulator/general/device.hpp"

namespace esp_brookesia::hal {

class BoardInfoSimulatorImpl: public BoardInfoIface {
public:
    BoardInfoSimulatorImpl()
        : BoardInfoIface(Info{
        .name = "ESP-Brookesia Simulator",
        .chip = "PC",
        .version = "sim-1",
        .description = "PC HAL simulator",
        .manufacturer = "Espressif",
    })
    {
    }
};

bool GeneralSimulatorDevice::probe()
{
    return true;
}

bool GeneralSimulatorDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        board_info_iface_ = std::make_shared<BoardInfoSimulatorImpl>(), false,
        "Failed to create board info simulator"
    );
    BROOKESIA_CHECK_FALSE_RETURN(board_info_iface_->get_info().is_valid(), false, "Board info simulator is invalid");

    interfaces_.emplace(BOARD_INFO_IFACE_NAME, board_info_iface_);

    return true;
}

void GeneralSimulatorDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BOARD_INFO_IFACE_NAME);
    board_info_iface_.reset();
}

#if BROOKESIA_HAL_SIMULATOR_ENABLE_GENERAL_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, GeneralSimulatorDevice, GeneralSimulatorDevice::DEVICE_NAME, GeneralSimulatorDevice::get_instance(),
    BROOKESIA_HAL_SIMULATOR_GENERAL_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
