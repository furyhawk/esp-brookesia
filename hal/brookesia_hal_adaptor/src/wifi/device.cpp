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

namespace {

class WifiBasicAdaptorIface: public WifiBasicIface {
public:
    explicit WifiBasicAdaptorIface(std::shared_ptr<WifiEspBackend> backend)
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
    std::shared_ptr<WifiEspBackend> backend_;
};

class WifiStaAdaptorIface: public WifiStaIface {
public:
    explicit WifiStaAdaptorIface(std::shared_ptr<WifiEspBackend> backend)
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
    std::shared_ptr<WifiEspBackend> backend_;
};

class WifiSoftApAdaptorIface: public WifiSoftApIface {
public:
    explicit WifiSoftApAdaptorIface(std::shared_ptr<WifiEspBackend> backend)
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
    std::shared_ptr<WifiEspBackend> backend_;
};

} // namespace

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

    interfaces_.emplace(BASIC_IFACE_NAME, std::make_shared<WifiBasicAdaptorIface>(backend_));
    interfaces_.emplace(STA_IFACE_NAME, std::make_shared<WifiStaAdaptorIface>(backend_));
    interfaces_.emplace(SOFTAP_IFACE_NAME, std::make_shared<WifiSoftApAdaptorIface>(backend_));

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
