/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <mutex>

#include "brookesia/hal_simulator/macro_configs.h"
#if !BROOKESIA_HAL_SIMULATOR_POWER_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_simulator/power/device.hpp"

namespace esp_brookesia::hal {

namespace {

PowerBatteryIface::Info make_battery_info()
{
    return PowerBatteryIface::Info{
        .name = "Simulator battery",
        .chemistry = "Li-ion",
        .abilities = {
            PowerBatteryIface::Ability::Voltage,
            PowerBatteryIface::Ability::Percentage,
            PowerBatteryIface::Ability::PowerSource,
            PowerBatteryIface::Ability::ChargeState,
            PowerBatteryIface::Ability::VbusVoltage,
            PowerBatteryIface::Ability::ChargerControl,
            PowerBatteryIface::Ability::ChargeConfig,
        },
    };
}

PowerBatteryIface::State make_battery_state()
{
    return PowerBatteryIface::State{
        .is_present = true,
        .power_source = PowerBatteryIface::PowerSource::External,
        .charge_state = PowerBatteryIface::ChargeState::ConstantCurrent,
        .level_source = PowerBatteryIface::LevelSource::FuelGauge,
        .voltage_mv = 3850,
        .percentage = 67,
        .vbus_voltage_mv = 5000,
        .system_voltage_mv = std::nullopt,
        .is_low = false,
        .is_critical = false,
    };
}

PowerBatteryIface::ChargeConfig make_charge_config()
{
    return PowerBatteryIface::ChargeConfig{
        .enabled = true,
        .target_voltage_mv = 4100,
        .charge_current_ma = 400,
        .precharge_current_ma = 50,
        .termination_current_ma = 25,
    };
}

} // namespace

class PowerBatterySimulatorImpl: public PowerBatteryIface {
public:
    PowerBatterySimulatorImpl()
        : PowerBatteryIface(make_battery_info())
        , state_(make_battery_state())
        , charge_config_(make_charge_config())
    {
    }

    bool get_state(State &state) override
    {
        std::lock_guard lock(mutex_);
        state = state_;
        return true;
    }

    bool get_charge_config(ChargeConfig &config) override
    {
        std::lock_guard lock(mutex_);
        config = charge_config_;
        return true;
    }

    bool set_charge_config(const ChargeConfig &config) override
    {
        std::lock_guard lock(mutex_);
        charge_config_ = config;
        state_.charge_state = config.enabled ? ChargeState::ConstantCurrent : ChargeState::NotCharging;
        return true;
    }

    bool set_charging_enabled(bool enabled) override
    {
        std::lock_guard lock(mutex_);
        charge_config_.enabled = enabled;
        state_.charge_state = enabled ? ChargeState::ConstantCurrent : ChargeState::NotCharging;
        return true;
    }

private:
    std::mutex mutex_;
    State state_{};
    ChargeConfig charge_config_{};
};

bool PowerSimulatorDevice::probe()
{
    return true;
}

bool PowerSimulatorDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        battery_iface_ = std::make_shared<PowerBatterySimulatorImpl>(), false,
        "Failed to create power battery simulator"
    );

    interfaces_.emplace(BATTERY_IFACE_NAME, battery_iface_);

    return true;
}

void PowerSimulatorDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BATTERY_IFACE_NAME);
    battery_iface_.reset();
}

#if BROOKESIA_HAL_SIMULATOR_ENABLE_POWER_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, PowerSimulatorDevice, PowerSimulatorDevice::DEVICE_NAME, PowerSimulatorDevice::get_instance(),
    BROOKESIA_HAL_SIMULATOR_POWER_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
