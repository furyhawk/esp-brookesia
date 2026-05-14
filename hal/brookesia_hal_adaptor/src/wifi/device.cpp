/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_adaptor/wifi/device.hpp"
#include "backend.hpp"

namespace esp_brookesia::hal {

bool WifiDevice::probe()
{
    return true;
}

bool WifiDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        backend_ = std::make_shared<WifiEspBackend>(), false, "Failed to create Wi-Fi ESP backend"
    );

    interfaces_.emplace(BASIC_IFACE_NAME, std::static_pointer_cast<WifiBasicIface>(backend_));
    interfaces_.emplace(STA_IFACE_NAME, std::static_pointer_cast<WifiStaIface>(backend_));
    interfaces_.emplace(SOFTAP_IFACE_NAME, std::static_pointer_cast<WifiSoftApIface>(backend_));

    return true;
}

void WifiDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BASIC_IFACE_NAME);
    interfaces_.erase(STA_IFACE_NAME);
    interfaces_.erase(SOFTAP_IFACE_NAME);
    backend_.reset();
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_WIFI_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, WifiDevice, WifiDevice::DEVICE_NAME, WifiDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
