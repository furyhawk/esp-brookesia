/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <memory>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

/**
 * @file basic.hpp
 * @brief Declares the basic Wi-Fi controller HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Basic Wi-Fi controller interface for shared lifecycle and runtime context.
 */
class WifiBasicIface: public Interface {
public:
    static constexpr const char *NAME = "WifiBasic"; ///< Interface registry name.

    struct RuntimeContext {
        std::shared_ptr<lib_utils::TaskScheduler> task_scheduler; ///< Scheduler used for async Wi-Fi work.
        lib_utils::TaskScheduler::Group task_group;               ///< Scheduler group used by the Wi-Fi service.
    };

    struct Callbacks {
        std::function<void(WifiBasicAction)> on_action; ///< Called when a basic action is triggered.
        std::function<void(WifiBasicEvent, bool)> on_event; ///< Called when a basic event happens.
        std::function<void()> on_error; ///< Called when a basic action fails.
    };

    WifiBasicIface()
        : Interface(NAME)
    {
    }

    virtual ~WifiBasicIface() = default;

    virtual bool configure(RuntimeContext runtime, Callbacks callbacks) = 0;
    virtual void clear_callbacks() = 0;

    virtual bool init() = 0;
    virtual void deinit() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void reset_data() = 0;

    virtual bool do_basic_action(WifiBasicAction action, bool is_force = false) = 0;
    virtual bool is_basic_action_running(WifiBasicAction action) = 0;
    virtual bool is_basic_event_ready(WifiBasicEvent event) = 0;
};

} // namespace esp_brookesia::hal
