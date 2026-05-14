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

TEST_CASE("WifiSoftApIface: init_device registers softap interface", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestWifiDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::WifiSoftApIface>();
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::SOFTAP_NAME, name.c_str());
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestWifiDevice::NAME);
}

TEST_CASE("WifiSoftApIface: params actions and callbacks", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestWifiDevice::NAME));

    auto [basic_name, basic_iface] = hal::get_first_interface<hal::WifiBasicIface>();
    auto [softap_name, softap_iface] = hal::get_first_interface<hal::WifiSoftApIface>();
    TEST_ASSERT_NOT_NULL(basic_iface.get());
    TEST_ASSERT_NOT_NULL(softap_iface.get());
    TEST_ASSERT_FALSE(basic_name.empty());
    TEST_ASSERT_FALSE(softap_name.empty());

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

    hal::WifiSoftApEvent last_event = hal::WifiSoftApEvent::Max;
    hal::WifiSoftApParams updated_params{};
    hal::WifiStaAction requested_action = hal::WifiStaAction::Max;

    TEST_ASSERT_TRUE(softap_iface->configure({
        .on_event = [&](hal::WifiSoftApEvent event)
        {
            last_event = event;
        },
        .on_params_updated = [&](const hal::WifiSoftApParams & params)
        {
            updated_params = params;
        },
        .on_sta_action_requested = [&](hal::WifiStaAction action)
        {
            requested_action = action;
            return true;
        },
    }));

    hal::WifiSoftApParams softap_params{
        .ssid = "SoftAP",
        .password = "12345678",
        .max_connection = 3,
        .channel = 6,
    };
    TEST_ASSERT_TRUE(softap_iface->set_softap_params(softap_params));
    TEST_ASSERT_EQUAL_STRING("SoftAP", softap_iface->get_softap_params().ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("SoftAP", updated_params.ssid.c_str());

    TEST_ASSERT_TRUE(softap_iface->start_softap());
    TEST_ASSERT_EQUAL(hal::WifiSoftApEvent::Started, last_event);

    softap_iface->stop_softap();
    TEST_ASSERT_EQUAL(hal::WifiSoftApEvent::Stopped, last_event);

    TEST_ASSERT_TRUE(softap_iface->start_softap_provision());
    TEST_ASSERT_EQUAL(hal::WifiSoftApEvent::Started, last_event);
    TEST_ASSERT_EQUAL(hal::WifiStaAction::Disconnect, requested_action);

    TEST_ASSERT_TRUE(softap_iface->stop_softap_provision());
    TEST_ASSERT_EQUAL(hal::WifiSoftApEvent::Stopped, last_event);

    softap_iface->clear_callbacks();
    basic_iface->deinit();
    hal::deinit_device(TestWifiDevice::NAME);
}
