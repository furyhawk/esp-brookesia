/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <memory>

#include "unity.h"
#include "common.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "test_wifi_common.hpp"

using namespace esp_brookesia;

TEST_CASE("WifiStaIface: init_device registers station interface", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestWifiDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::WifiStaIface>();
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::STA_NAME, name.c_str());
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestWifiDevice::NAME);
}

TEST_CASE("WifiStaIface: connect scan params and callbacks", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestWifiDevice::NAME));

    auto [basic_name, basic_iface] = hal::get_first_interface<hal::WifiBasicIface>();
    auto [sta_name, sta_iface] = hal::get_first_interface<hal::WifiStaIface>();
    TEST_ASSERT_NOT_NULL(basic_iface.get());
    TEST_ASSERT_NOT_NULL(sta_iface.get());
    TEST_ASSERT_FALSE(basic_name.empty());
    TEST_ASSERT_FALSE(sta_name.empty());

    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    TEST_ASSERT_TRUE(basic_iface->configure(
    hal::WifiBasicIface::RuntimeContext{
        .task_scheduler = scheduler,
        .task_group = "wifi-test",
    },
    {}
                     ));
    TEST_ASSERT_TRUE(basic_iface->init());
    TEST_ASSERT_TRUE(basic_iface->start());

    hal::WifiStaAction last_action = hal::WifiStaAction::Max;
    hal::WifiStaEvent last_event = hal::WifiStaEvent::Max;
    bool scan_state = false;
    size_t scan_ap_count = 0;
    hal::WifiConnectApInfo last_connected_ap{};
    hal::WifiConnectApInfoList connected_aps{};
    int error_count = 0;

    TEST_ASSERT_TRUE(sta_iface->configure({
        .on_action = [&](hal::WifiStaAction action)
        {
            last_action = action;
        },
        .on_event = [&](hal::WifiStaEvent event, bool)
        {
            last_event = event;
        },
        .on_error = [&]()
        {
            error_count++;
        },
        .on_action_requested = nullptr,
        .on_scan_state_changed = [&](bool is_scanning)
        {
            scan_state = is_scanning;
        },
        .on_scan_ap_infos_updated = [&](std::span<const hal::WifiScanApInfo> ap_infos)
        {
            scan_ap_count = ap_infos.size();
        },
        .on_last_connected_ap_info_updated = [&](const hal::WifiConnectApInfo & ap_info)
        {
            last_connected_ap = ap_info;
        },
        .on_connected_ap_infos_updated = [&](const hal::WifiConnectApInfoList & ap_infos)
        {
            connected_aps = ap_infos;
        },
    }));

    hal::WifiConnectApInfo target_ap("TestSSID", "TestPassword");
    TEST_ASSERT_TRUE(sta_iface->set_target_connect_ap_info(target_ap));
    TEST_ASSERT_EQUAL_STRING(target_ap.ssid.c_str(), sta_iface->get_target_connect_ap_info().ssid.c_str());

    hal::WifiConnectApInfo last_ap("PrevSSID", "PrevPassword");
    TEST_ASSERT_TRUE(sta_iface->set_last_connected_ap_info(last_ap));
    TEST_ASSERT_EQUAL_STRING(last_ap.ssid.c_str(), sta_iface->get_last_connected_ap_info().ssid.c_str());

    hal::WifiConnectApInfoList ap_list = {
        hal::WifiConnectApInfo("AP1", "PW1"),
        hal::WifiConnectApInfo("AP2", "PW2"),
    };
    TEST_ASSERT_TRUE(sta_iface->set_connected_ap_infos(ap_list));
    TEST_ASSERT_EQUAL(2, sta_iface->get_connected_ap_infos().size());

    sta_iface->mark_target_connect_ap_info_connectable(false);
    TEST_ASSERT_FALSE(sta_iface->get_target_connect_ap_info().is_connectable);

    hal::WifiScanParams scan_params{
        .ap_count = 4,
        .interval_ms = 1000,
        .timeout_ms = 2000,
    };
    TEST_ASSERT_TRUE(sta_iface->set_scan_params(scan_params));
    TEST_ASSERT_EQUAL(scan_params.ap_count, sta_iface->get_scan_params().ap_count);
    TEST_ASSERT_EQUAL(scan_params.interval_ms, sta_iface->get_scan_params().interval_ms);
    TEST_ASSERT_EQUAL(scan_params.timeout_ms, sta_iface->get_scan_params().timeout_ms);

    TEST_ASSERT_TRUE(sta_iface->start_scan());
    TEST_ASSERT_TRUE(scan_state);
    TEST_ASSERT_EQUAL(2, scan_ap_count);
    sta_iface->stop_scan();
    TEST_ASSERT_FALSE(scan_state);

    TEST_ASSERT_TRUE(sta_iface->do_sta_action(hal::WifiStaAction::Connect));
    TEST_ASSERT_EQUAL(hal::WifiStaAction::Connect, last_action);
    TEST_ASSERT_EQUAL(hal::WifiStaEvent::Connected, last_event);
    TEST_ASSERT_TRUE(sta_iface->is_sta_event_ready(hal::WifiStaEvent::Connected));
    TEST_ASSERT_EQUAL_STRING("TestSSID", last_connected_ap.ssid.c_str());
    TEST_ASSERT_EQUAL(3, connected_aps.size());

    TEST_ASSERT_TRUE(sta_iface->do_sta_action(hal::WifiStaAction::Disconnect));
    TEST_ASSERT_EQUAL(hal::WifiStaAction::Disconnect, last_action);
    TEST_ASSERT_EQUAL(hal::WifiStaEvent::Disconnected, last_event);
    TEST_ASSERT_TRUE(sta_iface->is_sta_event_ready(hal::WifiStaEvent::Disconnected));
    TEST_ASSERT_EQUAL(0, error_count);

    sta_iface->clear_callbacks();
    basic_iface->deinit();
    hal::deinit_device(TestWifiDevice::NAME);
}
