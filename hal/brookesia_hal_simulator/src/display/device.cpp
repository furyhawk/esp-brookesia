/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

#include "brookesia/hal_simulator/macro_configs.h"
#if !BROOKESIA_HAL_SIMULATOR_DISPLAY_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/hal_simulator/display/device.hpp"

namespace esp_brookesia::hal {

namespace {

DisplayPanelIface::Info make_panel_info()
{
    return DisplayPanelIface::Info{
        .h_res = 320,
        .v_res = 240,
        .pixel_format = DisplayPanelIface::PixelFormat::RGB565,
    };
}

DisplayTouchIface::Info make_touch_info()
{
    return DisplayTouchIface::Info{
        .x_max = 320,
        .y_max = 240,
        .operation_mode = DisplayTouchIface::OperationMode::Polling,
    };
}

} // namespace

class DisplayPanelSimulatorImpl: public DisplayPanelIface {
public:
    DisplayPanelSimulatorImpl()
        : DisplayPanelIface(make_panel_info())
    {
    }

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) override
    {
        std::lock_guard lock(mutex_);
        const auto &info = get_info();
        if ((data == nullptr) || (x2 <= x1) || (y2 <= y1) || (x2 > info.h_res) || (y2 > info.v_res)) {
            BROOKESIA_LOGE("Invalid panel draw region: (%1%, %2%) -> (%3%, %4%)", x1, y1, x2, y2);
            return false;
        }

        last_draw_region_ = {x1, y1, x2, y2};
        draw_count_++;
        return true;
    }

    bool get_driver_specific(DriverSpecific &specific) override
    {
        specific = DriverSpecific{
            .io_handle = nullptr,
            .panel_handle = nullptr,
            .bus_type = BusType::Generic,
            .draw_x_align_bytes = 1,
            .draw_y_align_bytes = 1,
        };
        return true;
    }

private:
    struct DrawRegion {
        uint32_t x1 = 0;
        uint32_t y1 = 0;
        uint32_t x2 = 0;
        uint32_t y2 = 0;
    };

    std::mutex mutex_;
    DrawRegion last_draw_region_{};
    size_t draw_count_ = 0;
};

class DisplayTouchSimulatorImpl: public DisplayTouchIface {
public:
    DisplayTouchSimulatorImpl()
        : DisplayTouchIface(make_touch_info())
    {
    }

    bool read_points(std::vector<Point> &points) override
    {
        points = {
            Point{.x = 12, .y = 34, .pressure = 56},
            Point{.x = 78, .y = 90, .pressure = 12},
        };
        return true;
    }

    bool register_interrupt_handler(InterruptHandler handler) override
    {
        interrupt_handler_ = std::move(handler);
        return true;
    }

    bool get_driver_specific(DriverSpecific &specific) override
    {
        specific = DriverSpecific{
            .io_handle = nullptr,
            .touch_handle = nullptr,
        };
        return true;
    }
};

class DisplayBacklightSimulatorImpl: public DisplayBacklightIface {
public:
    bool set_brightness(uint8_t percent) override
    {
        std::lock_guard lock(mutex_);
        brightness_ = percent;
        return true;
    }

    bool get_brightness(uint8_t &percent) override
    {
        std::lock_guard lock(mutex_);
        percent = brightness_;
        return true;
    }

    bool is_light_on_off_supported() override
    {
        return true;
    }

    bool set_light_on_off(bool on) override
    {
        std::lock_guard lock(mutex_);
        is_light_on_ = on;
        return true;
    }

    bool is_light_on() const override
    {
        std::lock_guard lock(mutex_);
        return is_light_on_;
    }

private:
    mutable std::mutex mutex_;
    uint8_t brightness_ = 0;
    bool is_light_on_ = false;
};

bool DisplaySimulatorDevice::probe()
{
    return true;
}

bool DisplaySimulatorDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        panel_iface_ = std::make_shared<DisplayPanelSimulatorImpl>(), false, "Failed to create display panel simulator"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        touch_iface_ = std::make_shared<DisplayTouchSimulatorImpl>(), false, "Failed to create display touch simulator"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        backlight_iface_ = std::make_shared<DisplayBacklightSimulatorImpl>(), false,
        "Failed to create display backlight simulator"
    );

    interfaces_.emplace(PANEL_IFACE_NAME, panel_iface_);
    interfaces_.emplace(TOUCH_IFACE_NAME, touch_iface_);
    interfaces_.emplace(BACKLIGHT_IFACE_NAME, backlight_iface_);

    return true;
}

void DisplaySimulatorDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BACKLIGHT_IFACE_NAME);
    interfaces_.erase(TOUCH_IFACE_NAME);
    interfaces_.erase(PANEL_IFACE_NAME);
    backlight_iface_.reset();
    touch_iface_.reset();
    panel_iface_.reset();
}

#if BROOKESIA_HAL_SIMULATOR_ENABLE_DISPLAY_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, DisplaySimulatorDevice, DisplaySimulatorDevice::DEVICE_NAME, DisplaySimulatorDevice::get_instance(),
    BROOKESIA_HAL_SIMULATOR_DISPLAY_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
