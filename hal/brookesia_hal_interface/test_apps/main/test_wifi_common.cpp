/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <utility>

#include "unity.h"
#include "test_wifi_common.hpp"

using namespace esp_brookesia;

namespace {

hal::WifiScanApInfoList make_scan_ap_infos()
{
    return hal::WifiScanApInfoList{
        hal::WifiScanApInfo{
            .ssid = "TestAP-1",
            .is_locked = true,
            .rssi = -42,
            .signal_level = hal::WifiScanApSignalLevel::LEVEL_4,
            .channel = 1,
        },
        hal::WifiScanApInfo{
            .ssid = "TestAP-2",
            .is_locked = false,
            .rssi = -72,
            .signal_level = hal::WifiScanApSignalLevel::LEVEL_1,
            .channel = 6,
        },
    };
}

} // namespace

namespace esp_brookesia {

bool TestWifiBackend::configure(hal::WifiBasicIface::RuntimeContext runtime, hal::WifiBasicIface::Callbacks callbacks)
{
    if (!runtime.task_scheduler) {
        return false;
    }

    runtime_ = std::move(runtime);
    basic_callbacks_ = std::move(callbacks);
    basic_configured_ = true;
    return true;
}

bool TestWifiBackend::configure(hal::WifiStaIface::Callbacks callbacks)
{
    sta_callbacks_ = std::move(callbacks);
    return true;
}

bool TestWifiBackend::configure(hal::WifiSoftApIface::Callbacks callbacks)
{
    softap_callbacks_ = std::move(callbacks);
    return true;
}

void TestWifiBackend::clear_callbacks()
{
    basic_callbacks_ = {};
    sta_callbacks_ = {};
    softap_callbacks_ = {};
}

bool TestWifiBackend::init()
{
    initialized_ = true;
    return true;
}

void TestWifiBackend::deinit()
{
    reset_data();
    initialized_ = false;
    started_ = false;
    basic_configured_ = false;
    running_basic_action_ = hal::WifiBasicAction::Max;
    running_sta_action_ = hal::WifiStaAction::Max;
    runtime_ = {};
    clear_callbacks();
}

bool TestWifiBackend::start()
{
    if (!initialized_ || !basic_configured_) {
        return false;
    }

    started_ = true;
    return true;
}

void TestWifiBackend::stop()
{
    started_ = false;
    connected_ = false;
    scanning_ = false;
    softap_started_ = false;
    provision_started_ = false;
}

void TestWifiBackend::reset_data()
{
    target_ap_ = {};
    last_ap_ = {};
    connected_aps_.clear();
    scan_params_ = {};
    scan_ap_infos_.clear();
    softap_params_ = {};
    connected_ = false;
    scanning_ = false;
    softap_started_ = false;
    provision_started_ = false;
}

bool TestWifiBackend::do_basic_action(hal::WifiBasicAction action, bool)
{
    running_basic_action_ = action;
    if (basic_callbacks_.on_action) {
        basic_callbacks_.on_action(action);
    }

    switch (action) {
    case hal::WifiBasicAction::Init:
        initialized_ = true;
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::WifiBasicEvent::Inited, false);
        }
        break;
    case hal::WifiBasicAction::Deinit:
        stop();
        initialized_ = false;
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::WifiBasicEvent::Deinited, false);
        }
        break;
    case hal::WifiBasicAction::Start:
        if (!initialized_) {
            if (basic_callbacks_.on_error) {
                basic_callbacks_.on_error();
            }
            running_basic_action_ = hal::WifiBasicAction::Max;
            return false;
        }
        started_ = true;
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::WifiBasicEvent::Started, false);
        }
        break;
    case hal::WifiBasicAction::Stop:
        stop();
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::WifiBasicEvent::Stopped, false);
        }
        break;
    default:
        if (basic_callbacks_.on_error) {
            basic_callbacks_.on_error();
        }
        running_basic_action_ = hal::WifiBasicAction::Max;
        return false;
    }

    running_basic_action_ = hal::WifiBasicAction::Max;
    return true;
}

bool TestWifiBackend::is_basic_action_running(hal::WifiBasicAction action)
{
    return running_basic_action_ == action;
}

bool TestWifiBackend::is_basic_event_ready(hal::WifiBasicEvent event)
{
    switch (event) {
    case hal::WifiBasicEvent::Inited:
        return initialized_;
    case hal::WifiBasicEvent::Deinited:
        return !initialized_;
    case hal::WifiBasicEvent::Started:
        return started_;
    case hal::WifiBasicEvent::Stopped:
        return !started_;
    default:
        return false;
    }
}

bool TestWifiBackend::do_sta_action(hal::WifiStaAction action, bool)
{
    running_sta_action_ = action;
    if (sta_callbacks_.on_action) {
        sta_callbacks_.on_action(action);
    }

    switch (action) {
    case hal::WifiStaAction::Connect:
        if (!target_ap_.is_valid()) {
            if (sta_callbacks_.on_error) {
                sta_callbacks_.on_error();
            }
            running_sta_action_ = hal::WifiStaAction::Max;
            return false;
        }
        connected_ = true;
        last_ap_ = target_ap_;
        if (sta_callbacks_.on_last_connected_ap_info_updated) {
            sta_callbacks_.on_last_connected_ap_info_updated(last_ap_);
        }
        connected_aps_.remove_if([this](const hal::WifiConnectApInfo & info) {
            return info.ssid == last_ap_.ssid;
        });
        connected_aps_.push_back(last_ap_);
        if (sta_callbacks_.on_connected_ap_infos_updated) {
            sta_callbacks_.on_connected_ap_infos_updated(connected_aps_);
        }
        if (sta_callbacks_.on_event) {
            sta_callbacks_.on_event(hal::WifiStaEvent::Connected, false);
        }
        break;
    case hal::WifiStaAction::Disconnect:
        connected_ = false;
        if (sta_callbacks_.on_event) {
            sta_callbacks_.on_event(hal::WifiStaEvent::Disconnected, false);
        }
        break;
    default:
        if (sta_callbacks_.on_error) {
            sta_callbacks_.on_error();
        }
        running_sta_action_ = hal::WifiStaAction::Max;
        return false;
    }

    running_sta_action_ = hal::WifiStaAction::Max;
    return true;
}

bool TestWifiBackend::is_sta_action_running(hal::WifiStaAction action)
{
    return running_sta_action_ == action;
}

bool TestWifiBackend::is_sta_event_ready(hal::WifiStaEvent event)
{
    switch (event) {
    case hal::WifiStaEvent::Connected:
        return connected_;
    case hal::WifiStaEvent::Disconnected:
        return !connected_;
    default:
        return false;
    }
}

void TestWifiBackend::mark_target_connect_ap_info_connectable(bool connectable)
{
    target_ap_.is_connectable = connectable;
}

bool TestWifiBackend::set_target_connect_ap_info(const hal::WifiConnectApInfo &ap_info)
{
    target_ap_ = ap_info;
    return true;
}

bool TestWifiBackend::set_last_connected_ap_info(const hal::WifiConnectApInfo &ap_info)
{
    last_ap_ = ap_info;
    return true;
}

bool TestWifiBackend::set_connected_ap_infos(const hal::WifiConnectApInfoList &ap_infos)
{
    connected_aps_ = ap_infos;
    return true;
}

const hal::WifiConnectApInfo &TestWifiBackend::get_target_connect_ap_info() const
{
    return target_ap_;
}

const hal::WifiConnectApInfo &TestWifiBackend::get_last_connected_ap_info() const
{
    return last_ap_;
}

const hal::WifiConnectApInfoList &TestWifiBackend::get_connected_ap_infos() const
{
    return connected_aps_;
}

bool TestWifiBackend::set_scan_params(const hal::WifiScanParams &params)
{
    scan_params_ = params;
    return true;
}

const hal::WifiScanParams &TestWifiBackend::get_scan_params() const
{
    return scan_params_;
}

bool TestWifiBackend::start_scan()
{
    scanning_ = true;
    scan_ap_infos_ = make_scan_ap_infos();
    if (sta_callbacks_.on_scan_state_changed) {
        sta_callbacks_.on_scan_state_changed(true);
    }
    if (sta_callbacks_.on_scan_ap_infos_updated) {
        sta_callbacks_.on_scan_ap_infos_updated(std::span<const hal::WifiScanApInfo>(scan_ap_infos_));
    }
    return true;
}

void TestWifiBackend::stop_scan()
{
    scanning_ = false;
    if (sta_callbacks_.on_scan_state_changed) {
        sta_callbacks_.on_scan_state_changed(false);
    }
}

bool TestWifiBackend::set_softap_params(const hal::WifiSoftApParams &params)
{
    softap_params_ = params;
    if (softap_callbacks_.on_params_updated) {
        softap_callbacks_.on_params_updated(softap_params_);
    }
    return true;
}

const hal::WifiSoftApParams &TestWifiBackend::get_softap_params() const
{
    return softap_params_;
}

bool TestWifiBackend::start_softap()
{
    softap_started_ = true;
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::WifiSoftApEvent::Started);
    }
    return true;
}

void TestWifiBackend::stop_softap()
{
    softap_started_ = false;
    provision_started_ = false;
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::WifiSoftApEvent::Stopped);
    }
}

bool TestWifiBackend::start_softap_provision()
{
    softap_started_ = true;
    provision_started_ = true;
    if (softap_callbacks_.on_sta_action_requested) {
        softap_callbacks_.on_sta_action_requested(hal::WifiStaAction::Disconnect);
    }
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::WifiSoftApEvent::Started);
    }
    return true;
}

bool TestWifiBackend::stop_softap_provision()
{
    provision_started_ = false;
    softap_started_ = false;
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::WifiSoftApEvent::Stopped);
    }
    return true;
}

bool TestWifiDevice::probe()
{
    return true;
}

bool TestWifiDevice::on_init()
{
    backend_ = std::make_shared<TestWifiBackend>();
    interfaces_.emplace(TestWifiBackend::BASIC_NAME, std::static_pointer_cast<hal::WifiBasicIface>(backend_));
    interfaces_.emplace(TestWifiBackend::STA_NAME, std::static_pointer_cast<hal::WifiStaIface>(backend_));
    interfaces_.emplace(TestWifiBackend::SOFTAP_NAME, std::static_pointer_cast<hal::WifiSoftApIface>(backend_));
    return true;
}

void TestWifiDevice::on_deinit()
{
    interfaces_.clear();
    backend_.reset();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestWifiDevice, std::string(TestWifiDevice::NAME));
