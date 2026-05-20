/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <span>
#include <utility>

#include "brookesia/hal_simulator/macro_configs.h"
#if !BROOKESIA_HAL_SIMULATOR_WIFI_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/sta.hpp"
#include "brookesia/hal_simulator/wifi/device.hpp"

namespace esp_brookesia::hal {

class WifiSimulatorBackend {
public:
    bool configure(WifiBasicIface::RuntimeContext runtime, WifiBasicIface::Callbacks callbacks)
    {
        task_scheduler_ = std::move(runtime.task_scheduler);
        task_group_ = runtime.task_group;
        basic_callbacks_ = std::move(callbacks);
        return true;
    }

    bool configure(WifiStaIface::Callbacks callbacks)
    {
        sta_callbacks_ = std::move(callbacks);
        return true;
    }

    bool configure(WifiSoftApIface::Callbacks callbacks)
    {
        softap_callbacks_ = std::move(callbacks);
        return true;
    }

    void clear_basic_callbacks()
    {
        basic_callbacks_ = {};
    }

    void clear_sta_callbacks()
    {
        sta_callbacks_ = {};
    }

    void clear_softap_callbacks()
    {
        softap_callbacks_ = {};
    }

    void clear_callbacks()
    {
        clear_basic_callbacks();
        clear_sta_callbacks();
        clear_softap_callbacks();
    }

    bool init()
    {
        backend_initialized_ = true;
        return true;
    }

    void deinit()
    {
        connected_ = false;
        started_ = false;
        initialized_ = false;
        backend_initialized_ = false;
        running_basic_action_ = WifiBasicAction::Max;
        running_sta_action_ = WifiStaAction::Max;
        clear_callbacks();
    }

    bool start()
    {
        if (!backend_initialized_) {
            return false;
        }
        is_running_ = true;
        return true;
    }

    void stop()
    {
        stop_scan();
        stop_softap();
        is_running_ = false;
        started_ = false;
        connected_ = false;
    }

    void reset_data()
    {
        target_ap_ = {};
        last_ap_ = {};
        connected_aps_.clear();
        scan_params_ = {};
        scan_ap_infos_.clear();
        softap_params_ = {};
    }

    bool do_basic_action(WifiBasicAction action, bool is_force = false)
    {
        if (!is_running_) {
            return false;
        }
        if ((running_basic_action_ == action) || (!is_force && is_basic_event_ready(get_target_event(action)))) {
            return true;
        }

        running_basic_action_ = action;
        if (basic_callbacks_.on_action) {
            basic_callbacks_.on_action(action);
        }

        switch (action) {
        case WifiBasicAction::Init:
            initialized_ = true;
            notify_basic_event(WifiBasicEvent::Inited, false);
            break;
        case WifiBasicAction::Deinit:
            stop();
            initialized_ = false;
            notify_basic_event(WifiBasicEvent::Deinited, false);
            break;
        case WifiBasicAction::Start:
            initialized_ = true;
            started_ = true;
            notify_basic_event(WifiBasicEvent::Started, false);
            break;
        case WifiBasicAction::Stop:
            connected_ = false;
            started_ = false;
            notify_basic_event(WifiBasicEvent::Stopped, false);
            break;
        default:
            running_basic_action_ = WifiBasicAction::Max;
            return false;
        }

        running_basic_action_ = WifiBasicAction::Max;
        return true;
    }

    bool is_basic_action_running(WifiBasicAction action)
    {
        return running_basic_action_ == action;
    }

    bool is_basic_event_ready(WifiBasicEvent event)
    {
        switch (event) {
        case WifiBasicEvent::Inited:
            return initialized_;
        case WifiBasicEvent::Deinited:
            return !initialized_;
        case WifiBasicEvent::Started:
            return started_;
        case WifiBasicEvent::Stopped:
            return !started_;
        default:
            return false;
        }
    }

    bool do_sta_action(WifiStaAction action, bool is_force = false)
    {
        if (!is_running_) {
            return false;
        }
        if ((running_sta_action_ == action) || (!is_force && is_sta_event_ready(get_target_event(action)))) {
            return true;
        }

        running_sta_action_ = action;
        if (sta_callbacks_.on_action) {
            sta_callbacks_.on_action(action);
        }

        switch (action) {
        case WifiStaAction::Connect:
            if (!target_ap_.is_valid()) {
                notify_sta_error();
                running_sta_action_ = WifiStaAction::Max;
                return false;
            }
            initialized_ = true;
            started_ = true;
            connected_ = true;
            last_ap_ = target_ap_;
            last_ap_.is_connectable = true;
            add_connected_ap(last_ap_);
            if (sta_callbacks_.on_last_connected_ap_info_updated) {
                sta_callbacks_.on_last_connected_ap_info_updated(last_ap_);
            }
            notify_sta_event(WifiStaEvent::Connected, false);
            break;
        case WifiStaAction::Disconnect:
            connected_ = false;
            notify_sta_event(WifiStaEvent::Disconnected, false);
            break;
        default:
            running_sta_action_ = WifiStaAction::Max;
            return false;
        }

        running_sta_action_ = WifiStaAction::Max;
        return true;
    }

    bool is_sta_action_running(WifiStaAction action)
    {
        return running_sta_action_ == action;
    }

    bool is_sta_event_ready(WifiStaEvent event)
    {
        switch (event) {
        case WifiStaEvent::Connected:
            return connected_;
        case WifiStaEvent::Disconnected:
            return !connected_;
        default:
            return false;
        }
    }

    void mark_target_connect_ap_info_connectable(bool connectable)
    {
        target_ap_.is_connectable = connectable;
    }

    bool set_target_connect_ap_info(const WifiConnectApInfo &ap_info)
    {
        target_ap_ = ap_info;
        return true;
    }

    bool set_last_connected_ap_info(const WifiConnectApInfo &ap_info)
    {
        last_ap_ = ap_info;
        return true;
    }

    bool set_connected_ap_infos(const WifiConnectApInfoList &ap_infos)
    {
        connected_aps_ = ap_infos;
        return true;
    }

    const WifiConnectApInfo &get_target_connect_ap_info() const
    {
        return target_ap_;
    }

    const WifiConnectApInfo &get_last_connected_ap_info() const
    {
        return last_ap_;
    }

    const WifiConnectApInfoList &get_connected_ap_infos() const
    {
        return connected_aps_;
    }

    bool set_scan_params(const WifiScanParams &params)
    {
        scan_params_ = params;
        return true;
    }

    const WifiScanParams &get_scan_params() const
    {
        return scan_params_;
    }

    bool start_scan()
    {
        if (is_scanning_) {
            return true;
        }

        is_scanning_ = true;
        if (sta_callbacks_.on_scan_state_changed) {
            sta_callbacks_.on_scan_state_changed(true);
        }

        scan_ap_infos_ = {
            WifiScanApInfo{
                .ssid = target_ap_.is_valid() ? target_ap_.ssid : std::string("Brookesia-Simulated-AP"),
                .is_locked = true,
                .rssi = -42,
                .signal_level = WifiScanApInfo::get_signal_level(-42),
                .channel = 6,
            },
            WifiScanApInfo{
                .ssid = "Brookesia-Guest",
                .is_locked = false,
                .rssi = -67,
                .signal_level = WifiScanApInfo::get_signal_level(-67),
                .channel = 11,
            },
        };
        auto publish_count = std::min(scan_ap_infos_.size(), scan_params_.ap_count);
        std::span<const WifiScanApInfo> ap_infos(scan_ap_infos_.data(), publish_count);
        if (sta_callbacks_.on_scan_ap_infos_updated) {
            sta_callbacks_.on_scan_ap_infos_updated(ap_infos);
        }

        is_scanning_ = false;
        if (sta_callbacks_.on_scan_state_changed) {
            sta_callbacks_.on_scan_state_changed(false);
        }

        return true;
    }

    void stop_scan()
    {
        if (!is_scanning_) {
            return;
        }
        is_scanning_ = false;
        if (sta_callbacks_.on_scan_state_changed) {
            sta_callbacks_.on_scan_state_changed(false);
        }
    }

    bool set_softap_params(const WifiSoftApParams &params)
    {
        softap_params_ = params;
        return true;
    }

    const WifiSoftApParams &get_softap_params() const
    {
        return softap_params_;
    }

    bool start_softap()
    {
        softap_started_ = true;
        if (softap_callbacks_.on_event) {
            softap_callbacks_.on_event(WifiSoftApEvent::Started);
        }
        return true;
    }

    void stop_softap()
    {
        if (!softap_started_) {
            return;
        }
        softap_started_ = false;
        if (softap_callbacks_.on_event) {
            softap_callbacks_.on_event(WifiSoftApEvent::Stopped);
        }
    }

    bool start_softap_provision()
    {
        provision_running_ = true;
        return start_softap();
    }

    bool stop_softap_provision()
    {
        provision_running_ = false;
        stop_softap();
        return true;
    }

private:
    static WifiBasicEvent get_target_event(WifiBasicAction action)
    {
        switch (action) {
        case WifiBasicAction::Init:
            return WifiBasicEvent::Inited;
        case WifiBasicAction::Deinit:
            return WifiBasicEvent::Deinited;
        case WifiBasicAction::Start:
            return WifiBasicEvent::Started;
        case WifiBasicAction::Stop:
            return WifiBasicEvent::Stopped;
        default:
            return WifiBasicEvent::Max;
        }
    }

    static WifiStaEvent get_target_event(WifiStaAction action)
    {
        switch (action) {
        case WifiStaAction::Connect:
            return WifiStaEvent::Connected;
        case WifiStaAction::Disconnect:
            return WifiStaEvent::Disconnected;
        default:
            return WifiStaEvent::Max;
        }
    }

    void notify_basic_event(WifiBasicEvent event, bool is_unexpected)
    {
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(event, is_unexpected);
        }
    }

    void notify_sta_event(WifiStaEvent event, bool is_unexpected)
    {
        if (sta_callbacks_.on_event) {
            sta_callbacks_.on_event(event, is_unexpected);
        }
    }

    void notify_sta_error()
    {
        if (sta_callbacks_.on_error) {
            sta_callbacks_.on_error();
        }
    }

    void add_connected_ap(const WifiConnectApInfo &ap_info)
    {
        for (auto &stored_ap_info : connected_aps_) {
            if (stored_ap_info.ssid == ap_info.ssid) {
                stored_ap_info = ap_info;
                if (sta_callbacks_.on_connected_ap_infos_updated) {
                    sta_callbacks_.on_connected_ap_infos_updated(connected_aps_);
                }
                return;
            }
        }

        connected_aps_.push_back(ap_info);
        if (sta_callbacks_.on_connected_ap_infos_updated) {
            sta_callbacks_.on_connected_ap_infos_updated(connected_aps_);
        }
    }

    bool initialized_ = false;
    bool backend_initialized_ = false;
    bool is_running_ = false;
    bool started_ = false;
    bool connected_ = false;
    bool is_scanning_ = false;
    bool softap_started_ = false;
    bool provision_running_ = false;
    WifiBasicAction running_basic_action_ = WifiBasicAction::Max;
    WifiStaAction running_sta_action_ = WifiStaAction::Max;
    WifiConnectApInfo target_ap_{};
    WifiConnectApInfo last_ap_{};
    WifiConnectApInfoList connected_aps_;
    WifiScanParams scan_params_{};
    WifiScanApInfoList scan_ap_infos_;
    WifiSoftApParams softap_params_{};
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    lib_utils::TaskScheduler::Group task_group_;
    WifiBasicIface::Callbacks basic_callbacks_;
    WifiStaIface::Callbacks sta_callbacks_;
    WifiSoftApIface::Callbacks softap_callbacks_;
};

namespace {

class WifiSimulatorBasicIface: public WifiBasicIface {
public:
    explicit WifiSimulatorBasicIface(std::shared_ptr<WifiSimulatorBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(RuntimeContext runtime, Callbacks callbacks) override
    {
        return backend_->configure(std::move(runtime), std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_basic_callbacks();
    }

    bool init() override
    {
        return backend_->init();
    }

    void deinit() override
    {
        backend_->deinit();
    }

    bool start() override
    {
        return backend_->start();
    }

    void stop() override
    {
        backend_->stop();
    }

    void reset_data() override
    {
        backend_->reset_data();
    }

    bool do_basic_action(WifiBasicAction action, bool is_force = false) override
    {
        return backend_->do_basic_action(action, is_force);
    }

    bool is_basic_action_running(WifiBasicAction action) override
    {
        return backend_->is_basic_action_running(action);
    }

    bool is_basic_event_ready(WifiBasicEvent event) override
    {
        return backend_->is_basic_event_ready(event);
    }

private:
    std::shared_ptr<WifiSimulatorBackend> backend_;
};

class WifiSimulatorStaIface: public WifiStaIface {
public:
    explicit WifiSimulatorStaIface(std::shared_ptr<WifiSimulatorBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_sta_callbacks();
    }

    bool do_sta_action(WifiStaAction action, bool is_force = false) override
    {
        return backend_->do_sta_action(action, is_force);
    }

    bool is_sta_action_running(WifiStaAction action) override
    {
        return backend_->is_sta_action_running(action);
    }

    bool is_sta_event_ready(WifiStaEvent event) override
    {
        return backend_->is_sta_event_ready(event);
    }

    void mark_target_connect_ap_info_connectable(bool connectable) override
    {
        backend_->mark_target_connect_ap_info_connectable(connectable);
    }

    bool set_target_connect_ap_info(const WifiConnectApInfo &ap_info) override
    {
        return backend_->set_target_connect_ap_info(ap_info);
    }

    bool set_last_connected_ap_info(const WifiConnectApInfo &ap_info) override
    {
        return backend_->set_last_connected_ap_info(ap_info);
    }

    bool set_connected_ap_infos(const WifiConnectApInfoList &ap_infos) override
    {
        return backend_->set_connected_ap_infos(ap_infos);
    }

    const WifiConnectApInfo &get_target_connect_ap_info() const override
    {
        return backend_->get_target_connect_ap_info();
    }

    const WifiConnectApInfo &get_last_connected_ap_info() const override
    {
        return backend_->get_last_connected_ap_info();
    }

    const WifiConnectApInfoList &get_connected_ap_infos() const override
    {
        return backend_->get_connected_ap_infos();
    }

    bool set_scan_params(const WifiScanParams &params) override
    {
        return backend_->set_scan_params(params);
    }

    const WifiScanParams &get_scan_params() const override
    {
        return backend_->get_scan_params();
    }

    bool start_scan() override
    {
        return backend_->start_scan();
    }

    void stop_scan() override
    {
        backend_->stop_scan();
    }

private:
    std::shared_ptr<WifiSimulatorBackend> backend_;
};

class WifiSimulatorSoftApIface: public WifiSoftApIface {
public:
    explicit WifiSimulatorSoftApIface(std::shared_ptr<WifiSimulatorBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_softap_callbacks();
    }

    bool set_softap_params(const WifiSoftApParams &params) override
    {
        return backend_->set_softap_params(params);
    }

    const WifiSoftApParams &get_softap_params() const override
    {
        return backend_->get_softap_params();
    }

    bool start_softap() override
    {
        return backend_->start_softap();
    }

    void stop_softap() override
    {
        backend_->stop_softap();
    }

    bool start_softap_provision() override
    {
        return backend_->start_softap_provision();
    }

    bool stop_softap_provision() override
    {
        return backend_->stop_softap_provision();
    }

private:
    std::shared_ptr<WifiSimulatorBackend> backend_;
};

} // namespace

bool WifiSimulatorDevice::probe()
{
    return true;
}

bool WifiSimulatorDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        backend_ = std::make_shared<WifiSimulatorBackend>(), false, "Failed to create Wi-Fi simulator backend"
    );

    interfaces_.emplace(BASIC_IFACE_NAME, std::make_shared<WifiSimulatorBasicIface>(backend_));
    interfaces_.emplace(STA_IFACE_NAME, std::make_shared<WifiSimulatorStaIface>(backend_));
    interfaces_.emplace(SOFTAP_IFACE_NAME, std::make_shared<WifiSimulatorSoftApIface>(backend_));

    return true;
}

void WifiSimulatorDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BASIC_IFACE_NAME);
    interfaces_.erase(STA_IFACE_NAME);
    interfaces_.erase(SOFTAP_IFACE_NAME);
    backend_.reset();
}

#if BROOKESIA_HAL_SIMULATOR_ENABLE_WIFI_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, WifiSimulatorDevice, WifiSimulatorDevice::DEVICE_NAME, WifiSimulatorDevice::get_instance(),
    BROOKESIA_HAL_SIMULATOR_WIFI_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
