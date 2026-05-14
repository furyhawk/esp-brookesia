/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include "boost/thread/thread.hpp"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_WIFI_BACKEND_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/nvs_flash_manager.hpp"
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "backend.hpp"

// Since it is necessary to call task_scheduler in wifi_event_handler, make sure the sys_event stack size is sufficient
#if CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE < 3072
#   error "`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` must be greater than `3072` to avoid stack overflow"
#endif

namespace esp_brookesia::hal {

namespace {
std::optional<WifiBasicAction> to_basic_action(WifiGeneralAction action)
{
    switch (action) {
    case WifiGeneralAction::Init:
        return WifiBasicAction::Init;
    case WifiGeneralAction::Deinit:
        return WifiBasicAction::Deinit;
    case WifiGeneralAction::Start:
        return WifiBasicAction::Start;
    case WifiGeneralAction::Stop:
        return WifiBasicAction::Stop;
    default:
        return std::nullopt;
    }
}

std::optional<WifiBasicEvent> to_basic_event(WifiGeneralEvent event)
{
    switch (event) {
    case WifiGeneralEvent::Inited:
        return WifiBasicEvent::Inited;
    case WifiGeneralEvent::Deinited:
        return WifiBasicEvent::Deinited;
    case WifiGeneralEvent::Started:
        return WifiBasicEvent::Started;
    case WifiGeneralEvent::Stopped:
        return WifiBasicEvent::Stopped;
    default:
        return std::nullopt;
    }
}

std::optional<WifiStaAction> to_sta_action(WifiGeneralAction action)
{
    switch (action) {
    case WifiGeneralAction::Connect:
        return WifiStaAction::Connect;
    case WifiGeneralAction::Disconnect:
        return WifiStaAction::Disconnect;
    default:
        return std::nullopt;
    }
}

std::optional<WifiStaEvent> to_sta_event(WifiGeneralEvent event)
{
    switch (event) {
    case WifiGeneralEvent::Connected:
        return WifiStaEvent::Connected;
    case WifiGeneralEvent::Disconnected:
        return WifiStaEvent::Disconnected;
    default:
        return std::nullopt;
    }
}

WifiGeneralAction to_general_action(WifiBasicAction action)
{
    switch (action) {
    case WifiBasicAction::Init:
        return WifiGeneralAction::Init;
    case WifiBasicAction::Deinit:
        return WifiGeneralAction::Deinit;
    case WifiBasicAction::Start:
        return WifiGeneralAction::Start;
    case WifiBasicAction::Stop:
        return WifiGeneralAction::Stop;
    default:
        return WifiGeneralAction::Max;
    }
}

WifiGeneralAction to_general_action(WifiStaAction action)
{
    switch (action) {
    case WifiStaAction::Connect:
        return WifiGeneralAction::Connect;
    case WifiStaAction::Disconnect:
        return WifiGeneralAction::Disconnect;
    default:
        return WifiGeneralAction::Max;
    }
}

wifi_sta_config_t get_sta_config(const WifiConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    wifi_sta_config_t cfg = {
        .ssid = "",
        .password = "",
        .scan_method = WIFI_FAST_SCAN,
        .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
        .threshold = {
            .rssi = -127,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
#pragma GCC diagnostic pop

    if (!ap_info.ssid.empty()) {
        snprintf(reinterpret_cast<char *>(cfg.ssid), sizeof(cfg.ssid), "%s", ap_info.ssid.c_str());
    }
    if (!ap_info.password.empty()) {
        snprintf(reinterpret_cast<char *>(cfg.password), sizeof(cfg.password), "%s", ap_info.password.c_str());
        cfg.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    return cfg;
}

WifiScanApInfo get_ap_info(const wifi_ap_record_t &ap_record)
{
    return WifiScanApInfo{
        .ssid = std::string(reinterpret_cast<const char *>(ap_record.ssid)),
        .is_locked = ap_record.authmode != WIFI_AUTH_OPEN,
        .rssi = ap_record.rssi,
        .signal_level = WifiScanApInfo::get_signal_level(ap_record.rssi),
        .channel = ap_record.primary,
    };
}
} // anonymous namespace

WifiEspBackend::~WifiEspBackend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    deinit();
}

bool WifiEspBackend::configure(WifiBasicIface::RuntimeContext runtime, WifiBasicIface::Callbacks callbacks)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(runtime.task_scheduler, false, "Invalid task scheduler");

    task_scheduler_ = std::move(runtime.task_scheduler);
    task_group_ = runtime.task_group;
    basic_callbacks_ = std::move(callbacks);

    return true;
}

bool WifiEspBackend::configure(WifiStaIface::Callbacks callbacks)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    sta_callbacks_ = std::move(callbacks);

    return true;
}

bool WifiEspBackend::configure(WifiSoftApIface::Callbacks callbacks)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    softap_callbacks_ = std::move(callbacks);

    return true;
}

void WifiEspBackend::clear_callbacks()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    basic_callbacks_ = {};
    sta_callbacks_ = {};
    softap_callbacks_ = {};
}

void WifiEspBackend::notify_general_action(WifiGeneralAction action)
{
    if (auto basic_action = to_basic_action(action); basic_action.has_value()) {
        if (basic_callbacks_.on_action) {
            basic_callbacks_.on_action(basic_action.value());
        }
        return;
    }

    if (auto sta_action = to_sta_action(action); sta_action.has_value() && sta_callbacks_.on_action) {
        sta_callbacks_.on_action(sta_action.value());
    }
}

void WifiEspBackend::notify_general_event(WifiGeneralEvent event, bool is_unexpected)
{
    if (auto basic_event = to_basic_event(event); basic_event.has_value()) {
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(basic_event.value(), is_unexpected);
        }
        return;
    }

    if (auto sta_event = to_sta_event(event); sta_event.has_value() && sta_callbacks_.on_event) {
        sta_callbacks_.on_event(sta_event.value(), is_unexpected);
    }
}

void WifiEspBackend::notify_error_state(WifiGeneralAction action)
{
    if (auto basic_action = to_basic_action(action); basic_action.has_value()) {
        if (basic_callbacks_.on_error) {
            basic_callbacks_.on_error();
        }
        return;
    }

    if ((to_sta_action(action).has_value()) && sta_callbacks_.on_error) {
        sta_callbacks_.on_error();
    }
}

bool WifiEspBackend::request_sta_action(WifiStaAction action)
{
    if (sta_callbacks_.on_action_requested) {
        return sta_callbacks_.on_action_requested(action);
    }
    if (softap_callbacks_.on_sta_action_requested) {
        return softap_callbacks_.on_sta_action_requested(action);
    }
    return false;
}

void WifiEspBackend::notify_scan_state_changed(bool is_scanning)
{
    if (sta_callbacks_.on_scan_state_changed) {
        sta_callbacks_.on_scan_state_changed(is_scanning);
    }
}

void WifiEspBackend::notify_scan_ap_infos_updated(std::span<const WifiScanApInfo> ap_infos)
{
    if (sta_callbacks_.on_scan_ap_infos_updated) {
        sta_callbacks_.on_scan_ap_infos_updated(ap_infos);
    }
}

void WifiEspBackend::notify_last_connected_ap_info_updated(const WifiConnectApInfo &ap_info)
{
    if (sta_callbacks_.on_last_connected_ap_info_updated) {
        sta_callbacks_.on_last_connected_ap_info_updated(ap_info);
    }
}

void WifiEspBackend::notify_connected_ap_infos_updated()
{
    if (sta_callbacks_.on_connected_ap_infos_updated) {
        sta_callbacks_.on_connected_ap_infos_updated(get_connected_ap_infos());
    }
}

void WifiEspBackend::notify_softap_event(WifiSoftApEvent event)
{
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(event);
    }
}

void WifiEspBackend::notify_softap_params_updated(const WifiSoftApParams &params)
{
    if (softap_callbacks_.on_params_updated) {
        softap_callbacks_.on_params_updated(params);
    }
}

bool WifiEspBackend::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit();
    });

    is_initialized_ = true;

    BROOKESIA_CHECK_FALSE_RETURN(nvs_flash_manager::acquire(), false, "Initialize NVS flash failed");

    esp_netif_init();

    auto result = esp_event_loop_create_default();
    if (result != ESP_ERR_INVALID_STATE) {
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Create default event loop failed");
    }

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        softap_ = std::make_unique<SoftAp>(this), false, "Create SoftAP failed"
    );

    deinit_guard.release();

    return true;
}

void WifiEspBackend::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized, skip");
        return;
    }

    if (is_running()) {
        BROOKESIA_LOGD("Running, stop it first");
        stop();
    }

    reset_data();

    // esp_netif_deinit();
    softap_.reset();
    nvs_flash_manager::release();

    is_initialized_ = false;
}

bool WifiEspBackend::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(task_scheduler_, false, "Invalid task scheduler");

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop();
    });

    is_running_ = true;

    auto wifi_ip_event_handler = +[](void *arg, esp_event_base_t base, int32_t id, void *data) {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGD("Params: arg(%1%), base(%2%), id(%3%), data(%4%)", arg, base, id, data);

        auto hal = static_cast<WifiEspBackend *>(arg);
        BROOKESIA_CHECK_NULL_EXIT(hal, "Invalid HAL");

        auto task_scheduler = hal->task_scheduler_;
        BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Invalid task scheduler");

        std::function<void()> task_func;
        if (base == WIFI_EVENT) {
            task_func = [hal, id = static_cast<uint8_t>(id)]() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_CHECK_FALSE_EXIT(hal->on_wifi_event(id), "Process WiFi event failed");
            };
        } else if (base == IP_EVENT) {
            task_func = [hal, id = static_cast<uint8_t>(id)]() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_CHECK_FALSE_EXIT(hal->on_ip_event(id), "Process IP event failed");
            };
        }
        if (task_func) {
            BROOKESIA_CHECK_FALSE_EXIT(
                task_scheduler->post(task_func, nullptr, hal->task_group_),
                "Post process event task failed"
            );
        }
    };
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_ip_event_handler, this,
            reinterpret_cast<esp_event_handler_instance_t *>(&wifi_event_handler_instance_)
        ), false, "Register WiFi event handler failed"
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_event_handler_instance_register(
            IP_EVENT, ESP_EVENT_ANY_ID, wifi_ip_event_handler, this,
            reinterpret_cast<esp_event_handler_instance_t *>(&ip_event_handler_instance_)
        ), false, "Register IP event handler failed"
    );

    stop_guard.release();

    return true;
}

void WifiEspBackend::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Try to stop and deinit WiFi
    BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(WifiGeneralAction::Stop), {}, {
        BROOKESIA_LOGE("Stop WiFi failed when deinit");
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(WifiGeneralAction::Deinit), {}, {
        BROOKESIA_LOGE("Deinit WiFi failed when deinit");
    });

    // Unregister event handlers
    if (wifi_event_handler_instance_ != nullptr) {
        auto result = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance_);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Unregister WiFi event handler failed");
        });
        wifi_event_handler_instance_ = nullptr;
    }
    if (ip_event_handler_instance_ != nullptr) {
        auto result = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler_instance_);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Unregister IP event handler failed");
        });
        ip_event_handler_instance_ = nullptr;
    }

    // Reset local variables
    task_scheduler_.reset();
    state_flags_.reset();

    is_running_ = false;
}

void WifiEspBackend::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    set_target_connect_ap_info(WifiConnectApInfo());
    set_last_connected_ap_info(WifiConnectApInfo());
    set_connected_ap_infos(std::list<WifiConnectApInfo>());
    set_scan_params(WifiScanParams());
    if (softap_ != nullptr) {
        softap_->reset_data();
    }
}

bool WifiEspBackend::do_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Check if netif already exists
    if (sta_netif_ != nullptr) {
        BROOKESIA_LOGD("Already initialized, directly trigger Inited event");
        trigger_general_event(WifiGeneralEvent::Inited);
        return true;
    }

    sta_netif_ = esp_netif_create_default_wifi_sta();
    BROOKESIA_CHECK_NULL_RETURN(sta_netif_, false, "Create default STA netif failed");

    auto init_func = [this, task_scheduler = task_scheduler_]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        BROOKESIA_CHECK_ESP_ERR_EXIT(esp_wifi_init(&cfg), "Initialize WiFi failed");
        BROOKESIA_CHECK_ESP_ERR_EXIT(esp_wifi_set_mode(WIFI_MODE_STA), "Set WiFi mode failed");

        auto task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(WifiGeneralEvent::Inited);
        };
        BROOKESIA_CHECK_FALSE_EXIT(
            task_scheduler->post(task_func, nullptr, task_group_),
            "Post init task failed"
        );
    };
    if (!lib_utils::ThreadConfig::check_stack_cache_safe()) {
        BROOKESIA_LOGD("Initialize WiFi in a thread");
        // Since initializing WiFi operates on Flash,
        // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        boost::thread(init_func).detach();
    } else {
        BROOKESIA_LOGD("Initialize WiFi in current thread");
        init_func();
    }

    return true;
}

bool WifiEspBackend::do_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if !CONFIG_ESP_HOSTED_ENABLED
    if (sta_netif_ != nullptr) {
        esp_netif_destroy_default_wifi(reinterpret_cast<esp_netif_t *>(sta_netif_));
        sta_netif_ = nullptr;
    }

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_deinit(), false, "Deinitialize WiFi failed");
#else
    BROOKESIA_LOGW("Not supported when hosted enabled, skip");
#endif
    trigger_general_event(WifiGeneralEvent::Deinited);

    return true;
}

bool WifiEspBackend::do_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_set_mode(WIFI_MODE_STA), false, "Set WiFi mode failed");
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_start(), false, "Failed to start WiFi");

    return true;
}

bool WifiEspBackend::do_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_stop(), false, "Stop WiFi failed");

    return true;
}

bool WifiEspBackend::do_connect(const WifiConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(ap_info.is_valid(), false, "Invalid connecting AP info");

    BROOKESIA_LOGI("Connecting to %1%...", ap_info.ssid);
    BROOKESIA_LOGD("With password: %1%", ap_info.password);

    wifi_config_t wifi_config = {
        .sta = get_sta_config(ap_info),
    };
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config), false, "Failed to set WiFi config"
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_connect(), false, "Failed to connect WiFi");

    return true;
}

bool WifiEspBackend::do_disconnect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_disconnect(), false, "Disconnect WiFi failed");

    return true;
}

bool WifiEspBackend::do_basic_action(WifiBasicAction action, bool is_force)
{
    auto general_action = to_general_action(action);
    BROOKESIA_CHECK_FALSE_RETURN(general_action != WifiGeneralAction::Max, false, "Invalid basic action");

    return do_general_action(general_action, is_force);
}

bool WifiEspBackend::is_basic_action_running(WifiBasicAction action)
{
    auto general_action = to_general_action(action);
    BROOKESIA_CHECK_FALSE_RETURN(general_action != WifiGeneralAction::Max, false, "Invalid basic action");

    return is_general_action_running(general_action);
}

bool WifiEspBackend::is_basic_event_ready(WifiBasicEvent event)
{
    switch (event) {
    case WifiBasicEvent::Inited:
        return is_general_event_ready(WifiGeneralEvent::Inited);
    case WifiBasicEvent::Deinited:
        return is_general_event_ready(WifiGeneralEvent::Deinited);
    case WifiBasicEvent::Started:
        return is_general_event_ready(WifiGeneralEvent::Started);
    case WifiBasicEvent::Stopped:
        return is_general_event_ready(WifiGeneralEvent::Stopped);
    default:
        return false;
    }
}

bool WifiEspBackend::do_sta_action(WifiStaAction action, bool is_force)
{
    auto general_action = to_general_action(action);
    BROOKESIA_CHECK_FALSE_RETURN(general_action != WifiGeneralAction::Max, false, "Invalid STA action");

    return do_general_action(general_action, is_force);
}

bool WifiEspBackend::is_sta_action_running(WifiStaAction action)
{
    auto general_action = to_general_action(action);
    BROOKESIA_CHECK_FALSE_RETURN(general_action != WifiGeneralAction::Max, false, "Invalid STA action");

    return is_general_action_running(general_action);
}

bool WifiEspBackend::is_sta_event_ready(WifiStaEvent event)
{
    switch (event) {
    case WifiStaEvent::Connected:
        return is_general_event_ready(WifiGeneralEvent::Connected);
    case WifiStaEvent::Disconnected:
        return is_general_event_ready(WifiGeneralEvent::Disconnected);
    default:
        return false;
    }
}

bool WifiEspBackend::do_scan_start(const void *params)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_scanning()) {
        BROOKESIA_LOGD("Scan is already running, skip");
        return true;
    }

    if (!is_general_event_ready(WifiGeneralEvent::Started) || is_general_action_running(WifiGeneralAction::Stop)) {
        BROOKESIA_LOGW("WiFi is not started or stopping, cancel start scan");
        return false;
    }

    if (is_general_action_running(WifiGeneralAction::Connect)) {
        BROOKESIA_LOGW("WiFi is connecting, skip AP scan");
        return true;
    }

    is_scanning_ = true;
    lib_utils::FunctionGuard scan_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        is_scanning_ = false;
    });

    lib_utils::FunctionGuard stop_scan_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        do_scan_stop();
    });

    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_scan_start(reinterpret_cast<const wifi_scan_config_t *>(params), false), false, "Start scan failed"
    );

    scan_guard.release();
    stop_scan_guard.release();

    return true;
}

bool WifiEspBackend::do_scan_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_scanning()) {
        BROOKESIA_LOGD("Scan is not running, skip");
        return true;
    }

    is_scanning_ = false;

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_scan_stop(), false, "Stop WiFi scan failed");

    return true;
}

bool WifiEspBackend::do_general_action(WifiGeneralAction action, bool is_force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto action_str = BROOKESIA_DESCRIBE_TO_STR(action);
    BROOKESIA_LOGD("Params: action(%1%), is_force(%2%)", action_str, is_force);

    if (is_general_action_running(action)) {
        BROOKESIA_LOGD("Action(%1%) is already running, skip", action_str);
        return true;
    }

    if (!is_force) {
        BROOKESIA_LOGD("Not force, check if the event is ready");
        WifiGeneralEvent target_event = get_general_action_target_event(action);
#if (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE) && (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG)
        BROOKESIA_LOGD(
            "running action(%1%)", BROOKESIA_DESCRIBE_TO_STR(get_running_general_action())
        );
        auto [event_flag_bit, event_bit_value] = get_general_event_state_flag_bit(target_event);
        BROOKESIA_LOGD(
            "state_flags(%1%), flag bit(%2%), bit value(%3%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_),
            BROOKESIA_DESCRIBE_TO_STR(event_flag_bit), BROOKESIA_DESCRIBE_TO_STR(event_bit_value)
        );
#endif
        if (is_general_event_ready(target_event)) {
            BROOKESIA_LOGD("Event(%1%) is already matched, skip", BROOKESIA_DESCRIBE_TO_STR(target_event));
            return true;
        }
    } else {
        BROOKESIA_LOGD("Force, skip checking event ready");
    }

    // Process special cases before doing the general action
    switch (action) {
    case WifiGeneralAction::Connect:
        do_scan_stop();
        break;
    case WifiGeneralAction::Stop:
        stop_scan();
        if (softap_ != nullptr) {
            softap_->stop_softap();
        }
        break;
    default:
        break;
    }

    // Notify action before executing `do_xxxx()`, since `do_xxxx()` may call trigger_general_event() directly.
    notify_general_action(action);

    BROOKESIA_LOGI("WiFi do '%1%' running...", action_str);

    // Action running bit should be cleared by call `trigger_general_event()` when the action is done or failed
    update_general_action_state_bit(action, true);
    lib_utils::FunctionGuard clear_action_guard([this, action]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        update_general_action_state_bit(action, false);
    });

    bool result = true;
    switch (action) {
    case WifiGeneralAction::Init:
        result = do_init();
        break;
    case WifiGeneralAction::Deinit:
        result = do_deinit();
        break;
    case WifiGeneralAction::Start:
        result = do_start();
        break;
    case WifiGeneralAction::Stop:
        result = do_stop();
        break;
    case WifiGeneralAction::Connect:
        result = do_connect(get_target_connect_ap_info());
        break;
    case WifiGeneralAction::Disconnect:
        result = do_disconnect();
        break;
    default:
        BROOKESIA_LOGE("Invalid action: %1%", action_str);
        result = false;
        break;
    }
    if (!result) {
        notify_error_state(action);
        BROOKESIA_LOGE("WiFi do '%1%' failed", action_str);
        return false;
    }

    clear_action_guard.release();

    BROOKESIA_LOGI("WiFi do '%1%' finished", action_str);

    return true;
}

void WifiEspBackend::trigger_general_event(WifiGeneralEvent event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto event_str = BROOKESIA_DESCRIBE_TO_STR(event);
    BROOKESIA_LOGD("Params: event(%1%)", event_str);

    if (is_unexpected_event_processing_) {
        BROOKESIA_LOGD("Unexpected event processing, skip");
        return;
    }

    if (is_general_event_ready(event)) {
        BROOKESIA_LOGD("Event(%1%) is already matched, skip", event_str);
        return;
    }

    auto event_action = get_general_action_from_target_event(event);
    BROOKESIA_CHECK_FALSE_EXIT(
        event_action != WifiGeneralAction::Max, "No corresponding action for event: %1%", event_str
    );

    auto is_unexpected_event = is_general_event_unexpected(event);
    if (is_unexpected_event) {
        BROOKESIA_LOGW("Detected unexpected event: %1%", event_str);

        // Clear the current action running bit
        auto running_action = get_running_general_action();
        update_general_action_state_bit(running_action, false);

        // Force do the event action to clear the unexpected event
        BROOKESIA_LOGW("Force to do the event action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        is_unexpected_event_processing_ = true;
        BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(event_action, true), {}, {
            BROOKESIA_LOGE("Failed to do general action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        });
        is_unexpected_event_processing_ = false;
    } else {
        BROOKESIA_LOGI("Detected expected event: %1%", event_str);
    }

    // Clear target event action running bit
    update_general_action_state_bit(event_action, false);
    // Set target event stable bit
    update_general_event_state_bit(event, true);

    notify_general_event(event, is_unexpected_event);
}

bool WifiEspBackend::set_scan_params(const WifiScanParams &params)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: params(%1%)", BROOKESIA_DESCRIBE_TO_STR(params));

    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.ap_count, 1, SIZE_MAX, false, "Invalid AP count");
    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.interval_ms, 1, UINT32_MAX, false, "Invalid interval");
    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.timeout_ms, params.interval_ms + 1, UINT32_MAX, false, "Invalid timeout");

    scan_params_ = params;

    return true;
}

bool WifiEspBackend::start_scan()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    bool is_scanning = is_scan_task_running();

    // Stop scan and delayed tasks
    stop_scan();

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_scan();
    });

    // Try to start scan immediately if WiFi is started and not stopping
    if (is_general_event_ready(WifiGeneralEvent::Started) && !is_general_action_running(WifiGeneralAction::Stop)) {
        BROOKESIA_LOGD("Do initial scan");
        BROOKESIA_CHECK_FALSE_RETURN(do_scan_start(), false, "Start scan failed");
    }

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Invalid task scheduler");

    auto &scan_params = get_scan_params();

    // Post periodic task to start next scan after interval
    auto periodic_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if (!is_general_event_ready(WifiGeneralEvent::Started) || is_general_action_running(WifiGeneralAction::Stop)) {
            BROOKESIA_LOGD("WiFi is not started or stopping, skip scan");
            return true;
        }

        if (!do_scan_start()) {
            BROOKESIA_LOGW("Start scan failed, stop scan");
            stop_scan();
            return false;
        }

        return true;
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler->post_periodic(
            std::move(periodic_task), scan_params.interval_ms, &scan_periodic_task_,
            task_group_
        ),
        false, "Post scan AP periodic task failed"
    );

    // Post delayed task to stop scan after timeout
    auto delayed_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_scan();
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler->post_delayed(
            std::move(delayed_task), scan_params.timeout_ms, &scan_ap_timeout_task_,
            task_group_
        ), false, "Post scan AP delayed task failed"
    );

    if (!is_scanning) {
        notify_scan_state_changed(true);
    }

    stop_guard.release();

    return true;
}

void WifiEspBackend::stop_scan()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    bool is_scanning = is_scan_task_running();

    if (scan_periodic_task_ != 0) {
        task_scheduler_->cancel(scan_periodic_task_);
        scan_periodic_task_ = 0;
    }
    if (scan_ap_timeout_task_ != 0) {
        task_scheduler_->cancel(scan_ap_timeout_task_);
        scan_ap_timeout_task_ = 0;
    }
    do_scan_stop();

    if (is_scanning) {
        notify_scan_state_changed(false);
    }
}

WifiGeneralEvent WifiEspBackend::get_general_action_target_event(WifiGeneralAction action)
{
    switch (action) {
    case WifiGeneralAction::Init:
        return WifiGeneralEvent::Inited;
    case WifiGeneralAction::Deinit:
        return WifiGeneralEvent::Deinited;
    case WifiGeneralAction::Start:
        return WifiGeneralEvent::Started;
    case WifiGeneralAction::Stop:
        return WifiGeneralEvent::Stopped;
    case WifiGeneralAction::Connect:
        return WifiGeneralEvent::Connected;
    case WifiGeneralAction::Disconnect:
        return WifiGeneralEvent::Disconnected;
    default:
        return WifiGeneralEvent::Max;
    }
}

WifiStateFlagBit WifiEspBackend::get_general_action_state_flag_bit(WifiGeneralAction action)
{
    switch (action) {
    case WifiGeneralAction::Init:
        return WifiStateFlagBit::Initing;
    case WifiGeneralAction::Deinit:
        return WifiStateFlagBit::Deiniting;
    case WifiGeneralAction::Start:
        return WifiStateFlagBit::Starting;
    case WifiGeneralAction::Stop:
        return WifiStateFlagBit::Stopping;
    case WifiGeneralAction::Connect:
        return WifiStateFlagBit::Connecting;
    case WifiGeneralAction::Disconnect:
        return WifiStateFlagBit::Disconnecting;
    default:
        return WifiStateFlagBit::Max;
    }
}

std::pair<WifiStateFlagBit, bool> WifiEspBackend::get_general_event_state_flag_bit(WifiGeneralEvent event)
{
    bool bit_value = true;
    WifiStateFlagBit flag_bit = WifiStateFlagBit::Max;
    switch (event) {
    case WifiGeneralEvent::Inited:
        flag_bit = WifiStateFlagBit::Inited;
        break;
    case WifiGeneralEvent::Deinited:
        flag_bit = WifiStateFlagBit::Inited;
        bit_value = false;
        break;
    case WifiGeneralEvent::Started:
        flag_bit = WifiStateFlagBit::Started;
        break;
    case WifiGeneralEvent::Stopped:
        flag_bit = WifiStateFlagBit::Started;
        bit_value = false;
        break;
    case WifiGeneralEvent::Connected:
        flag_bit = WifiStateFlagBit::Connected;
        break;
    case WifiGeneralEvent::Disconnected:
        flag_bit = WifiStateFlagBit::Connected;
        bit_value = false;
        break;
    default:
        break;
    }

    return {flag_bit, bit_value};
}

bool WifiEspBackend::set_connected_ap_infos(const std::list<WifiConnectApInfo> &ap_infos)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_infos(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_infos));

    connected_ap_info_list_ = ap_infos;

    return true;
}

bool WifiEspBackend::is_general_action_running(WifiGeneralAction action)
{
    WifiStateFlagBit flag_bit = get_general_action_state_flag_bit(action);
    BROOKESIA_CHECK_FALSE_RETURN(flag_bit != WifiStateFlagBit::Max, false, "Invalid action");

    return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit));
}

bool WifiEspBackend::is_general_event_conflicting(WifiGeneralEvent event)
{
    switch (event) {
    case WifiGeneralEvent::Deinited:
        return is_general_action_running(WifiGeneralAction::Init);
    case WifiGeneralEvent::Stopped:
        return is_general_action_running(WifiGeneralAction::Start);
    case WifiGeneralEvent::Disconnected:
        return is_general_action_running(WifiGeneralAction::Connect);
    case WifiGeneralEvent::Connected:
        return is_general_action_running(WifiGeneralAction::Disconnect);
    default:
        return false;
    }
}

bool WifiEspBackend::is_general_event_ready(WifiGeneralEvent event)
{
    // Check if the corresponding action is running
    if (is_general_action_running(get_general_action_from_target_event(event))) {
        // BROOKESIA_LOGD(
        //     "Event(%1%) is not ready because the corresponding action is running", BROOKESIA_DESCRIBE_TO_STR(event)
        // );
        return false;
    }

    // Check for conflicting state transitions: prevent events that contradict the currently running action
    if (is_general_event_conflicting(event)) {
        // BROOKESIA_LOGD(
        //     "Event(%1%) is not ready because it contradicts the currently running action",
        //     BROOKESIA_DESCRIBE_TO_STR(event)
        // );
        return false;
    }

    auto [flag_bit, bit_value] = get_general_event_state_flag_bit(event);
    BROOKESIA_CHECK_FALSE_RETURN(flag_bit != WifiStateFlagBit::Max, false, "Invalid event");

    return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit)) == bit_value;
}

void WifiEspBackend::update_general_action_state_bit(WifiGeneralAction action, bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: action(%1%), enable(%2%)", BROOKESIA_DESCRIBE_TO_STR(action), BROOKESIA_DESCRIBE_TO_STR(enable)
    );

    if (action == WifiGeneralAction::Max) {
        BROOKESIA_LOGD("Invalid action, skip");
        return;
    }

    auto flag_bit = get_general_action_state_flag_bit(action);
    if (flag_bit == WifiStateFlagBit::Max) {
        BROOKESIA_LOGD("Invalid action, skip");
        return;
    }

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit), enable);
}

bool WifiEspBackend::is_general_event_unexpected(WifiGeneralEvent event)
{
    auto event_action = get_general_action_from_target_event(event);
    if (event_action == WifiGeneralAction::Max) {
        return true;
    }

    auto running_action = get_running_general_action();
    if (running_action == WifiGeneralAction::Max) {
        return true;
    }

    if ((event == WifiGeneralEvent::Disconnected) && (running_action == WifiGeneralAction::Stop)) {
        return false;
    }

    return (running_action != event_action);
}

WifiGeneralAction WifiEspBackend::get_running_general_action()
{
    for (auto action = static_cast<WifiGeneralAction>(0); action < WifiGeneralAction::Max;
            action = static_cast<WifiGeneralAction>(static_cast<int>(action) + 1)) {
        auto flag_bit = get_general_action_state_flag_bit(action);
        // Only handle transient state flag bits
        BROOKESIA_CHECK_FALSE_RETURN(
            flag_bit != WifiStateFlagBit::Max, WifiGeneralAction::Max, "Invalid action: %1%",
            BROOKESIA_DESCRIBE_TO_STR(action)
        );
        if (state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit))) {
            return action;
        }
    }
    return WifiGeneralAction::Max;
}

WifiGeneralAction WifiEspBackend::get_general_action_from_target_event(WifiGeneralEvent event)
{
    switch (event) {
    case WifiGeneralEvent::Inited:
        return WifiGeneralAction::Init;
    case WifiGeneralEvent::Deinited:
        return WifiGeneralAction::Deinit;
    case WifiGeneralEvent::Started:
        return WifiGeneralAction::Start;
    case WifiGeneralEvent::Stopped:
        return WifiGeneralAction::Stop;
    case WifiGeneralEvent::Connected:
        return WifiGeneralAction::Connect;
    case WifiGeneralEvent::Disconnected:
        return WifiGeneralAction::Disconnect;
    default:
        return WifiGeneralAction::Max;
    }
}

void WifiEspBackend::update_general_event_state_bit(WifiGeneralEvent event, bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event(%1%), enable(%2%)", BROOKESIA_DESCRIBE_TO_STR(event), BROOKESIA_DESCRIBE_TO_STR(enable)
    );

    if (event == WifiGeneralEvent::Max) {
        BROOKESIA_LOGD("Invalid event, skip");
        return;
    }

    auto [flag_bit, bit_value] = get_general_event_state_flag_bit(event);
    if (flag_bit == WifiStateFlagBit::Max) {
        BROOKESIA_LOGD("Invalid event, skip");
        return;
    }

    BROOKESIA_LOGD(
        "Target bit(%1%), original state flags(%2%)", BROOKESIA_DESCRIBE_TO_STR(flag_bit),
        BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit), enable ? bit_value : !bit_value);

    BROOKESIA_LOGD(
        "New state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );
}

bool WifiEspBackend::set_target_connect_ap_info(const WifiConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    target_connect_ap_info_ = ap_info;

    return true;
}

void WifiEspBackend::mark_target_connect_ap_info_connectable(bool connectable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connectable(%1%)", BROOKESIA_DESCRIBE_TO_STR(connectable));

    target_connect_ap_info_.is_connectable = connectable;
}

bool WifiEspBackend::set_last_connected_ap_info(const WifiConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    last_connected_ap_info_ = ap_info;

    return true;
}

bool WifiEspBackend::on_wifi_event(uint8_t event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", event);

    auto running_action = get_running_general_action();
    auto event_id = static_cast<wifi_event_t>(event);
    bool skip_softap_event = false;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        trigger_general_event(WifiGeneralEvent::Started);
        break;
    case WIFI_EVENT_STA_STOP:
        trigger_general_event(WifiGeneralEvent::Stopped);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        trigger_general_event(WifiGeneralEvent::Disconnected);
        break;
    case WIFI_EVENT_SCAN_DONE: {
        is_scanning_ = false;
        // If WiFi is stopping or already stopped, skip update scan AP infos
        if ((running_action == WifiGeneralAction::Stop) || is_general_event_ready(WifiGeneralEvent::Stopped) ||
                (running_action == WifiGeneralAction::Deinit) || is_general_event_ready(WifiGeneralEvent::Deinited)) {
            break;
        }
        // Update scan AP infos
        BROOKESIA_CHECK_FALSE_EXECUTE(update_scan_ap_infos(), {}, {
            BROOKESIA_LOGE("Update scan AP infos failed");
        });
        break;
    }
    default:
        BROOKESIA_LOGD("Ignored");
    }

    // Handle SoftAP WiFi event
    if (skip_softap_event) {
        BROOKESIA_LOGD("Skip SoftAP WiFi event");
    } else if (softap_ != nullptr) {
        BROOKESIA_CHECK_FALSE_EXECUTE(softap_->on_hal_softap_wifi_event(event), {}, {
            BROOKESIA_LOGE("SoftAP failed to handle WiFi event");
        });
    }

    return true;
}

bool WifiEspBackend::on_ip_event(uint8_t event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", event);

    auto event_id = static_cast<ip_event_t>(event);
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
        trigger_general_event(WifiGeneralEvent::Connected);
        break;
    }
    default:
        BROOKESIA_LOGD("Ignored");
    }

    if (softap_ != nullptr) {
        BROOKESIA_CHECK_FALSE_EXECUTE(softap_->on_hal_softap_ip_event(event), {}, {
            BROOKESIA_LOGE("SoftAP failed to handle IP event");
        });
    }

    return true;
}

bool WifiEspBackend::update_scan_ap_infos()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    uint16_t actual_ap_count = 0;
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_scan_get_ap_num(&actual_ap_count), false, "Get AP number failed");

    if (actual_ap_count == 0) {
        BROOKESIA_LOGD("No AP found, skip");
        return true;
    }

    std::vector<wifi_ap_record_t> ap_records(actual_ap_count);
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_scan_get_ap_records(&actual_ap_count, ap_records.data()), false, "Get AP records failed"
    );

    BROOKESIA_LOGI("Scanned AP count: %1%", actual_ap_count);

    std::vector<WifiScanApInfo> scan_ap_infos(actual_ap_count);
    for (size_t i = 0; (i < actual_ap_count); i++) {
        const auto &ap_record = ap_records[i];
        scan_ap_infos[i] = get_ap_info(ap_record);
        BROOKESIA_LOGD("\t- Scanned AP[%1%]: %2%", i, BROOKESIA_DESCRIBE_TO_STR(scan_ap_infos[i]));
    }

    auto publish_ap_count = std::min(actual_ap_count, static_cast<uint16_t>(get_scan_params().ap_count));
    std::span<const WifiScanApInfo> publish_ap_infos(scan_ap_infos.begin(), publish_ap_count);
    notify_scan_ap_infos_updated(publish_ap_infos);

    // Handle scan AP infos updated
    if (softap_ != nullptr) {
        BROOKESIA_CHECK_FALSE_EXECUTE(softap_->on_hal_scan_ap_infos_updated(scan_ap_infos), {}, {
            BROOKESIA_LOGE("SoftAP failed to handle scan AP infos updated");
        });
    }

    return true;
}

} // namespace esp_brookesia::hal
