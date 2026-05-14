/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <list>
#include <span>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"

/**
 * @file sta.hpp
 * @brief Declares the Wi-Fi station HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Wi-Fi station interface for AP connection and scanning.
 */
class WifiStaIface: public Interface {
public:
    static constexpr const char *NAME = "WifiSta"; ///< Interface registry name.

    struct Callbacks {
        std::function<void(WifiStaAction)> on_action; ///< Called when a station action is triggered.
        std::function<void(WifiStaEvent, bool)> on_event; ///< Called when a station event happens.
        std::function<void()> on_error; ///< Called when a station action fails.
        std::function<bool(WifiStaAction)> on_action_requested; ///< Called when backend requests a station action.
        std::function<void(bool)> on_scan_state_changed; ///< Called when periodic scan state changes.
        std::function<void(std::span<const WifiScanApInfo>)> on_scan_ap_infos_updated; ///< Called with scan results.
        std::function<void(const WifiConnectApInfo &)> on_last_connected_ap_info_updated; ///< Persist last AP.
        std::function<void(const WifiConnectApInfoList &)> on_connected_ap_infos_updated; ///< Persist known APs.
    };

    WifiStaIface()
        : Interface(NAME)
    {
    }

    virtual ~WifiStaIface() = default;

    virtual bool configure(Callbacks callbacks) = 0;
    virtual void clear_callbacks() = 0;

    virtual bool do_sta_action(WifiStaAction action, bool is_force = false) = 0;
    virtual bool is_sta_action_running(WifiStaAction action) = 0;
    virtual bool is_sta_event_ready(WifiStaEvent event) = 0;

    virtual void mark_target_connect_ap_info_connectable(bool connectable) = 0;
    virtual bool set_target_connect_ap_info(const WifiConnectApInfo &ap_info) = 0;
    virtual bool set_last_connected_ap_info(const WifiConnectApInfo &ap_info) = 0;
    virtual bool set_connected_ap_infos(const WifiConnectApInfoList &ap_infos) = 0;
    virtual const WifiConnectApInfo &get_target_connect_ap_info() const = 0;
    virtual const WifiConnectApInfo &get_last_connected_ap_info() const = 0;
    virtual const WifiConnectApInfoList &get_connected_ap_infos() const = 0;

    virtual bool set_scan_params(const WifiScanParams &params) = 0;
    virtual const WifiScanParams &get_scan_params() const = 0;
    virtual bool start_scan() = 0;
    virtual void stop_scan() = 0;
};

} // namespace esp_brookesia::hal
