/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"

/**
 * @file softap.hpp
 * @brief Declares the Wi-Fi SoftAP HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Wi-Fi SoftAP interface for AP mode and provisioning.
 */
class WifiSoftApIface: public Interface {
public:
    static constexpr const char *NAME = "WifiSoftAp"; ///< Interface registry name.

    struct Callbacks {
        std::function<void(WifiSoftApEvent)> on_event; ///< Called when a SoftAP event happens.
        std::function<void(const WifiSoftApParams &)> on_params_updated; ///< Persist backend-updated params.
        std::function<bool(WifiStaAction)> on_sta_action_requested; ///< Called when SoftAP needs STA action.
    };

    WifiSoftApIface()
        : Interface(NAME)
    {
    }

    virtual ~WifiSoftApIface() = default;

    virtual bool configure(Callbacks callbacks) = 0;
    virtual void clear_callbacks() = 0;

    virtual bool set_softap_params(const WifiSoftApParams &params) = 0;
    virtual const WifiSoftApParams &get_softap_params() const = 0;
    virtual bool start_softap() = 0;
    virtual void stop_softap() = 0;
    virtual bool start_softap_provision() = 0;
    virtual bool stop_softap_provision() = 0;
};

} // namespace esp_brookesia::hal
