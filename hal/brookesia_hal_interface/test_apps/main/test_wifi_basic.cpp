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

TEST_CASE("WifiBasicIface: get_first_interface returns empty pair when no devices initialized", "[hal][interface]")
{
    auto [name, iface] = hal::get_first_interface<hal::WifiBasicIface>();
    TEST_ASSERT_TRUE(name.empty());
    TEST_ASSERT_NULL(iface.get());
}

TEST_CASE("WifiBasicIface: init_device registers basic interface", "[hal][device]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestWifiDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::WifiBasicIface>();
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::BASIC_NAME, name.c_str());
    TEST_ASSERT_NOT_NULL(iface.get());

    hal::deinit_device(TestWifiDevice::NAME);
}

TEST_CASE("WifiBasicIface: configure lifecycle and callbacks", "[hal][interface]")
{
    TEST_ASSERT_TRUE(hal::init_device(TestWifiDevice::NAME));

    auto [name, iface] = hal::get_first_interface<hal::WifiBasicIface>();
    TEST_ASSERT_NOT_NULL(iface.get());

    TEST_ASSERT_FALSE(iface->configure({}, {}));

    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    hal::WifiBasicAction last_action = hal::WifiBasicAction::Max;
    hal::WifiBasicEvent last_event = hal::WifiBasicEvent::Max;
    bool last_unexpected = true;
    int error_count = 0;

    TEST_ASSERT_TRUE(iface->configure(
    hal::WifiBasicIface::RuntimeContext{
        .task_scheduler = scheduler,
        .task_group = "wifi-test",
    },
    hal::WifiBasicIface::Callbacks{
        .on_action = [&](hal::WifiBasicAction action)
        {
            last_action = action;
        },
        .on_event = [&](hal::WifiBasicEvent event, bool is_unexpected)
        {
            last_event = event;
            last_unexpected = is_unexpected;
        },
        .on_error = [&]()
        {
            error_count++;
        },
    }
                     ));

    TEST_ASSERT_TRUE(iface->init());
    TEST_ASSERT_TRUE(iface->start());

    TEST_ASSERT_TRUE(iface->do_basic_action(hal::WifiBasicAction::Init));
    TEST_ASSERT_EQUAL(hal::WifiBasicAction::Init, last_action);
    TEST_ASSERT_EQUAL(hal::WifiBasicEvent::Inited, last_event);
    TEST_ASSERT_FALSE(last_unexpected);
    TEST_ASSERT_TRUE(iface->is_basic_event_ready(hal::WifiBasicEvent::Inited));

    TEST_ASSERT_TRUE(iface->do_basic_action(hal::WifiBasicAction::Start));
    TEST_ASSERT_EQUAL(hal::WifiBasicAction::Start, last_action);
    TEST_ASSERT_EQUAL(hal::WifiBasicEvent::Started, last_event);
    TEST_ASSERT_TRUE(iface->is_basic_event_ready(hal::WifiBasicEvent::Started));

    TEST_ASSERT_TRUE(iface->do_basic_action(hal::WifiBasicAction::Stop));
    TEST_ASSERT_EQUAL(hal::WifiBasicAction::Stop, last_action);
    TEST_ASSERT_EQUAL(hal::WifiBasicEvent::Stopped, last_event);
    TEST_ASSERT_TRUE(iface->is_basic_event_ready(hal::WifiBasicEvent::Stopped));

    TEST_ASSERT_TRUE(iface->do_basic_action(hal::WifiBasicAction::Deinit));
    TEST_ASSERT_EQUAL(hal::WifiBasicAction::Deinit, last_action);
    TEST_ASSERT_EQUAL(hal::WifiBasicEvent::Deinited, last_event);
    TEST_ASSERT_TRUE(iface->is_basic_event_ready(hal::WifiBasicEvent::Deinited));
    TEST_ASSERT_EQUAL(0, error_count);

    iface->clear_callbacks();
    TEST_ASSERT_TRUE(iface->configure(
    hal::WifiBasicIface::RuntimeContext{
        .task_scheduler = scheduler,
        .task_group = "wifi-test",
    },
    {}
                     ));
    TEST_ASSERT_TRUE(iface->do_basic_action(hal::WifiBasicAction::Init));
    iface->deinit();

    hal::deinit_device(TestWifiDevice::NAME);
}
