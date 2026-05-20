/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <list>
#include <memory>
#include <vector>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/sta.hpp"

namespace esp_brookesia {

class TestWifiBackend {
public:
    static constexpr const char *BASIC_NAME = "TestWiFi:Basic";
    static constexpr const char *STA_NAME = "TestWiFi:STA";
    static constexpr const char *SOFTAP_NAME = "TestWiFi:SoftAP";

    bool configure_basic(hal::WifiBasicIface::RuntimeContext runtime, hal::WifiBasicIface::Callbacks callbacks);
    bool configure_sta(hal::WifiStaIface::Callbacks callbacks);
    bool configure_softap(hal::WifiSoftApIface::Callbacks callbacks);
    void clear_basic_callbacks();
    void clear_sta_callbacks();
    void clear_softap_callbacks();
    void clear_all_callbacks();

    bool init();
    void deinit();
    bool start();
    void stop();
    void reset_data();

    bool do_basic_action(hal::WifiBasicAction action, bool is_force = false);
    bool is_basic_action_running(hal::WifiBasicAction action);
    bool is_basic_event_ready(hal::WifiBasicEvent event);

    bool do_sta_action(hal::WifiStaAction action, bool is_force = false);
    bool is_sta_action_running(hal::WifiStaAction action);
    bool is_sta_event_ready(hal::WifiStaEvent event);

    void mark_target_connect_ap_info_connectable(bool connectable);
    bool set_target_connect_ap_info(const hal::WifiConnectApInfo &ap_info);
    bool set_last_connected_ap_info(const hal::WifiConnectApInfo &ap_info);
    bool set_connected_ap_infos(const hal::WifiConnectApInfoList &ap_infos);
    const hal::WifiConnectApInfo &get_target_connect_ap_info() const;
    const hal::WifiConnectApInfo &get_last_connected_ap_info() const;
    const hal::WifiConnectApInfoList &get_connected_ap_infos() const;

    bool set_scan_params(const hal::WifiScanParams &params);
    const hal::WifiScanParams &get_scan_params() const;
    bool start_scan();
    void stop_scan();

    bool set_softap_params(const hal::WifiSoftApParams &params);
    const hal::WifiSoftApParams &get_softap_params() const;
    bool start_softap();
    void stop_softap();
    bool start_softap_provision();
    bool stop_softap_provision();

private:
    bool initialized_ = false;
    bool started_ = false;
    bool connected_ = false;
    bool scanning_ = false;
    bool softap_started_ = false;
    bool provision_started_ = false;
    bool basic_configured_ = false;
    hal::WifiBasicAction running_basic_action_ = hal::WifiBasicAction::Max;
    hal::WifiStaAction running_sta_action_ = hal::WifiStaAction::Max;
    hal::WifiBasicIface::RuntimeContext runtime_{};
    hal::WifiBasicIface::Callbacks basic_callbacks_{};
    hal::WifiStaIface::Callbacks sta_callbacks_{};
    hal::WifiSoftApIface::Callbacks softap_callbacks_{};
    hal::WifiConnectApInfo target_ap_{};
    hal::WifiConnectApInfo last_ap_{};
    hal::WifiConnectApInfoList connected_aps_{};
    hal::WifiScanParams scan_params_{};
    hal::WifiScanApInfoList scan_ap_infos_{};
    hal::WifiSoftApParams softap_params_{};
};

class TestWifiBasicIface: public hal::WifiBasicIface {
public:
    explicit TestWifiBasicIface(std::shared_ptr<TestWifiBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(RuntimeContext runtime, Callbacks callbacks) override;
    void clear_callbacks() override;

    bool init() override;
    void deinit() override;
    bool start() override;
    void stop() override;
    void reset_data() override;

    bool do_basic_action(hal::WifiBasicAction action, bool is_force = false) override;
    bool is_basic_action_running(hal::WifiBasicAction action) override;
    bool is_basic_event_ready(hal::WifiBasicEvent event) override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

class TestWifiStaIface: public hal::WifiStaIface {
public:
    explicit TestWifiStaIface(std::shared_ptr<TestWifiBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override;
    void clear_callbacks() override;

    bool do_sta_action(hal::WifiStaAction action, bool is_force = false) override;
    bool is_sta_action_running(hal::WifiStaAction action) override;
    bool is_sta_event_ready(hal::WifiStaEvent event) override;

    void mark_target_connect_ap_info_connectable(bool connectable) override;
    bool set_target_connect_ap_info(const hal::WifiConnectApInfo &ap_info) override;
    bool set_last_connected_ap_info(const hal::WifiConnectApInfo &ap_info) override;
    bool set_connected_ap_infos(const hal::WifiConnectApInfoList &ap_infos) override;
    const hal::WifiConnectApInfo &get_target_connect_ap_info() const override;
    const hal::WifiConnectApInfo &get_last_connected_ap_info() const override;
    const hal::WifiConnectApInfoList &get_connected_ap_infos() const override;

    bool set_scan_params(const hal::WifiScanParams &params) override;
    const hal::WifiScanParams &get_scan_params() const override;
    bool start_scan() override;
    void stop_scan() override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

class TestWifiSoftApIface: public hal::WifiSoftApIface {
public:
    explicit TestWifiSoftApIface(std::shared_ptr<TestWifiBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override;
    void clear_callbacks() override;

    bool set_softap_params(const hal::WifiSoftApParams &params) override;
    const hal::WifiSoftApParams &get_softap_params() const override;
    bool start_softap() override;
    void stop_softap() override;
    bool start_softap_provision() override;
    bool stop_softap_provision() override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

class TestWifiDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestWiFi";

    TestWifiDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

} // namespace esp_brookesia
