/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <cstdint>
#include <list>
#include <memory>
#include <span>
#include <string>
#include "brookesia/hal_adaptor/macro_configs.h"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/sta.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "softap.hpp"

namespace esp_brookesia::hal {

enum class WifiStateFlagBit {
    Idle,
    Initing,
    Inited,
    Deiniting,
    Starting,
    Started,
    Stopping,
    Connecting,
    Connected,
    Disconnecting,
    Max,
};

BROOKESIA_DESCRIBE_ENUM(
    WifiStateFlagBit, Idle, Initing, Inited, Deiniting, Starting, Started, Stopping, Connecting, Connected,
    Disconnecting, Max
);

using WifiStateFlags = std::bitset<BROOKESIA_DESCRIBE_ENUM_TO_NUM(WifiStateFlagBit::Max)>;

class WifiEspBackend {
public:
    friend class SoftAp;

    WifiEspBackend() = default;
    ~WifiEspBackend();

    bool configure(WifiBasicIface::RuntimeContext runtime, WifiBasicIface::Callbacks callbacks);
    bool configure(WifiStaIface::Callbacks callbacks);
    bool configure(WifiSoftApIface::Callbacks callbacks);
    void clear_basic_callbacks();
    void clear_sta_callbacks();
    void clear_softap_callbacks();
    void clear_callbacks();

    bool init();
    void deinit();
    bool start();
    void stop();
    void reset_data();

    bool do_basic_action(WifiBasicAction action, bool is_force = false);
    bool is_basic_action_running(WifiBasicAction action);
    bool is_basic_event_ready(WifiBasicEvent event);

    bool do_sta_action(WifiStaAction action, bool is_force = false);
    bool is_sta_action_running(WifiStaAction action);
    bool is_sta_event_ready(WifiStaEvent event);

    /**
     * @brief Connect related
     */
    void mark_target_connect_ap_info_connectable(bool connectable);
    bool set_target_connect_ap_info(const WifiConnectApInfo &ap_info);
    bool set_last_connected_ap_info(const WifiConnectApInfo &ap_info);
    bool set_connected_ap_infos(const WifiConnectApInfoList &ap_infos);
    const WifiConnectApInfo &get_target_connect_ap_info() const
    {
        return target_connect_ap_info_;
    }
    const WifiConnectApInfo &get_last_connected_ap_info() const
    {
        return last_connected_ap_info_;
    }
    const WifiConnectApInfoList &get_connected_ap_infos() const
    {
        return connected_ap_info_list_;
    }

    /**
     * @brief Scan related
     */
    bool set_scan_params(const WifiScanParams &params);
    bool start_scan();
    void stop_scan();
    const WifiScanParams &get_scan_params() const
    {
        return scan_params_;
    }

    /**
     * @brief SoftAP related
     */
    bool start_softap();
    void stop_softap();
    bool set_softap_params(const WifiSoftApParams &params);
    const WifiSoftApParams &get_softap_params() const;
    bool start_softap_provision();
    bool stop_softap_provision();

private:
    bool is_initialized()
    {
        return is_initialized_;
    }
    bool is_running()
    {
        return is_running_;
    }
    std::shared_ptr<lib_utils::TaskScheduler> get_task_scheduler()
    {
        return task_scheduler_;
    }
    lib_utils::TaskScheduler::Group get_task_group() const
    {
        return task_group_;
    }

    bool do_init();
    bool do_deinit();
    bool do_start();
    bool do_stop();
    bool do_connect(const WifiConnectApInfo &ap_info);
    bool do_disconnect();

    bool do_scan_start(const void *params = nullptr);
    bool do_scan_stop();
    bool is_scanning()
    {
        return is_scanning_;
    }
    bool is_scan_task_running()
    {
        return (scan_periodic_task_ != 0);
    }

    bool do_general_action(WifiGeneralAction action, bool is_force = false);
    void trigger_general_event(WifiGeneralEvent event);
    bool is_general_action_running(WifiGeneralAction action);
    bool is_general_event_ready(WifiGeneralEvent event);

    WifiGeneralEvent get_general_action_target_event(WifiGeneralAction action);
    bool is_general_event_unexpected(WifiGeneralEvent event);
    bool is_general_event_conflicting(WifiGeneralEvent event);
    WifiGeneralAction get_running_general_action();
    WifiGeneralAction get_general_action_from_target_event(WifiGeneralEvent event);

    WifiStateFlagBit get_general_action_state_flag_bit(WifiGeneralAction action);
    std::pair<WifiStateFlagBit, bool> get_general_event_state_flag_bit(WifiGeneralEvent event);
    void update_general_action_state_bit(WifiGeneralAction action, bool enable);
    void update_general_event_state_bit(WifiGeneralEvent event, bool enable);

    void notify_general_action(WifiGeneralAction action);
    void notify_general_event(WifiGeneralEvent event, bool is_unexpected);
    void notify_error_state(WifiGeneralAction action);
    bool request_sta_action(WifiStaAction action);
    void notify_scan_state_changed(bool is_scanning);
    void notify_scan_ap_infos_updated(std::span<const WifiScanApInfo> ap_infos);
    void notify_last_connected_ap_info_updated(const WifiConnectApInfo &ap_info);
    void notify_connected_ap_infos_updated();
    void notify_softap_event(WifiSoftApEvent event);
    void notify_softap_params_updated(const WifiSoftApParams &params);

    bool on_wifi_event(uint8_t event);
    bool on_ip_event(uint8_t event);

    bool update_scan_ap_infos();

    bool is_initialized_ = false;
    bool is_running_ = false;
    bool is_unexpected_event_processing_ = false;

    WifiStateFlags state_flags_;

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    lib_utils::TaskScheduler::Group task_group_;
    WifiBasicIface::Callbacks basic_callbacks_;
    WifiStaIface::Callbacks sta_callbacks_;
    WifiSoftApIface::Callbacks softap_callbacks_;

#if CONFIG_ESP_HOSTED_ENABLED
    // Hosted doesn't support to multiple create netif, so use a static variable to store the netif
    inline static void *sta_netif_ = nullptr;
#else
    void *sta_netif_ = nullptr;
#endif
    void *wifi_event_handler_instance_ = nullptr;
    void *ip_event_handler_instance_ = nullptr;
    WifiConnectApInfo target_connect_ap_info_{};
    WifiConnectApInfo last_connected_ap_info_{};
    WifiConnectApInfoList connected_ap_info_list_;

    bool is_scanning_ = false;
    WifiScanParams scan_params_{};
    lib_utils::TaskScheduler::TaskId scan_periodic_task_ = 0;
    lib_utils::TaskScheduler::TaskId scan_ap_timeout_task_ = 0;

    WifiSoftApParams softap_params_cache_{};
    std::unique_ptr<SoftAp> softap_;
};

} // namespace esp_brookesia::hal
