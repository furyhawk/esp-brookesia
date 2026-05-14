/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_simulator.hpp"

using namespace esp_brookesia;

namespace {

bool expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "[FAILED] " << message << std::endl;
    }

    return condition;
}

template<typename T>
std::shared_ptr<T> get_iface(const char *expected_name, bool &ok)
{
    auto [name, iface] = hal::get_first_interface<T>();
    ok &= expect(iface != nullptr, std::string(expected_name) + " is registered");
    ok &= expect(name == expected_name, std::string(expected_name) + " has expected registry name");

    return iface;
}

bool test_general()
{
    bool ok = true;
    auto iface = get_iface<hal::BoardInfoIface>(hal::GeneralSimulatorDevice::BOARD_INFO_IFACE_NAME, ok);
    if (!iface) {
        return false;
    }

    const auto &info = iface->get_info();
    ok &= expect(info.is_valid(), "board info is valid");
    ok &= expect(info.name == "ESP-Brookesia Simulator", "board name matches");
    ok &= expect(info.chip == "PC", "board chip matches");
    ok &= expect(info.version == "sim-1", "board version matches");
    ok &= expect(info.description == "PC HAL simulator", "board description matches");
    ok &= expect(info.manufacturer == "Espressif", "board manufacturer matches");

    return ok;
}

bool test_audio()
{
    bool ok = true;
    auto player = get_iface<hal::AudioCodecPlayerIface>(hal::AudioSimulatorDevice::PLAYER_IFACE_NAME, ok);
    auto recorder = get_iface<hal::AudioCodecRecorderIface>(hal::AudioSimulatorDevice::RECORDER_IFACE_NAME, ok);
    if (!player || !recorder) {
        return false;
    }

    ok &= expect(player->open(hal::AudioCodecPlayerIface::Config{
        .bits = 16,
        .channels = 1,
        .sample_rate = 16000,
    }), "audio player opens");
    ok &= expect(player->set_volume(42), "audio player sets volume");
    ok &= expect(player->is_pa_on_off_supported(), "audio player supports PA on/off");
    ok &= expect(player->set_pa_on_off(false), "audio player sets PA off");
    ok &= expect(!player->is_pa_on(), "audio player PA is off");

    std::array<uint8_t, 4> player_data = {0x11, 0x22, 0x33, 0x44};
    ok &= expect(player->write_data(player_data.data(), player_data.size()), "audio player writes payload");
    ok &= expect(!player->write_data(nullptr, player_data.size()), "audio player rejects null payload");
    player->close();

    const auto &recorder_info = recorder->get_info();
    ok &= expect(recorder_info.bits == 16, "audio recorder bits match");
    ok &= expect(recorder_info.channels == 2, "audio recorder channels match");
    ok &= expect(recorder_info.sample_rate == 16000, "audio recorder sample rate matches");
    ok &= expect(recorder_info.mic_layout == "LR", "audio recorder mic layout matches");
    ok &= expect(recorder->open(), "audio recorder opens");

    std::array<uint8_t, 8> recorder_data = {};
    ok &= expect(recorder->read_data(recorder_data.data(), recorder_data.size()), "audio recorder reads payload");
    ok &= expect(
    std::all_of(recorder_data.begin(), recorder_data.end(), [](uint8_t value) {
        return value == 0x5A;
    }),
    "audio recorder fills deterministic payload"
          );
    ok &= expect(!recorder->read_data(nullptr, recorder_data.size()), "audio recorder rejects null destination");
    recorder->close();

    return ok;
}

bool test_display()
{
    bool ok = true;
    auto panel = get_iface<hal::DisplayPanelIface>(hal::DisplaySimulatorDevice::PANEL_IFACE_NAME, ok);
    auto touch = get_iface<hal::DisplayTouchIface>(hal::DisplaySimulatorDevice::TOUCH_IFACE_NAME, ok);
    auto backlight = get_iface<hal::DisplayBacklightIface>(hal::DisplaySimulatorDevice::BACKLIGHT_IFACE_NAME, ok);
    if (!panel || !touch || !backlight) {
        return false;
    }

    const auto &panel_info = panel->get_info();
    ok &= expect(panel_info.h_res == 320, "display panel horizontal resolution matches");
    ok &= expect(panel_info.v_res == 240, "display panel vertical resolution matches");
    ok &= expect(
              panel_info.pixel_format == hal::DisplayPanelIface::PixelFormat::RGB565,
              "display panel format matches"
          );

    std::array<uint8_t, 8> pixels = {};
    ok &= expect(panel->draw_bitmap(0, 0, 2, 2, pixels.data()), "display panel draws valid bitmap");
    ok &= expect(!panel->draw_bitmap(0, 0, 321, 2, pixels.data()), "display panel rejects out-of-bounds draw");
    ok &= expect(!panel->draw_bitmap(0, 0, 2, 2, nullptr), "display panel rejects null bitmap");

    hal::DisplayPanelIface::DriverSpecific panel_specific = {};
    ok &= expect(panel->get_driver_specific(panel_specific), "display panel returns driver-specific placeholder");
    ok &= expect(panel_specific.bus_type == hal::DisplayPanelIface::BusType::Generic, "display panel bus is generic");
    ok &= expect(panel_specific.io_handle == nullptr, "display panel IO handle is not real");
    ok &= expect(panel_specific.panel_handle == nullptr, "display panel handle is not real");

    std::vector<hal::DisplayTouchIface::Point> points;
    ok &= expect(touch->read_points(points), "display touch reads points");
    ok &= expect(points.size() == 2, "display touch returns deterministic points");
    ok &= expect(points[0].x == 12 && points[0].y == 34 && points[0].pressure == 56, "first touch point matches");
    ok &= expect(points[1].x == 78 && points[1].y == 90 && points[1].pressure == 12, "second touch point matches");
    ok &= expect(touch->register_interrupt_handler([]() {}), "display touch registers interrupt handler");

    hal::DisplayTouchIface::DriverSpecific touch_specific = {};
    ok &= expect(touch->get_driver_specific(touch_specific), "display touch returns driver-specific placeholder");
    ok &= expect(touch_specific.io_handle == nullptr, "display touch IO handle is not real");
    ok &= expect(touch_specific.touch_handle == nullptr, "display touch handle is not real");

    ok &= expect(backlight->set_brightness(42), "display backlight sets brightness");
    uint8_t brightness = 0;
    ok &= expect(backlight->get_brightness(brightness), "display backlight gets brightness");
    ok &= expect(brightness == 42, "display backlight brightness matches");
    ok &= expect(backlight->is_light_on_off_supported(), "display backlight supports on/off");
    ok &= expect(backlight->set_light_on_off(true), "display backlight turns on");
    ok &= expect(backlight->is_light_on(), "display backlight is on");

    return ok;
}

bool test_power()
{
    bool ok = true;
    auto battery = get_iface<hal::PowerBatteryIface>(hal::PowerSimulatorDevice::BATTERY_IFACE_NAME, ok);
    if (!battery) {
        return false;
    }

    const auto &info = battery->get_info();
    ok &= expect(info.has_ability(hal::PowerBatteryIface::Ability::Voltage), "battery supports voltage");
    ok &= expect(info.has_ability(hal::PowerBatteryIface::Ability::Percentage), "battery supports percentage");
    ok &= expect(info.has_ability(hal::PowerBatteryIface::Ability::ChargerControl), "battery supports charger control");
    ok &= expect(info.has_ability(hal::PowerBatteryIface::Ability::ChargeConfig), "battery supports charge config");

    hal::PowerBatteryIface::State state = {};
    ok &= expect(battery->get_state(state), "battery gets state");
    ok &= expect(state.is_present, "battery is present");
    ok &= expect(state.power_source == hal::PowerBatteryIface::PowerSource::External, "battery power source matches");
    ok &= expect(
              state.charge_state == hal::PowerBatteryIface::ChargeState::ConstantCurrent,
              "battery charge state matches"
          );
    ok &= expect(state.voltage_mv == 3850, "battery voltage matches");
    ok &= expect(state.percentage == 67, "battery percentage matches");
    ok &= expect(state.vbus_voltage_mv == 5000, "battery VBUS voltage matches");

    hal::PowerBatteryIface::ChargeConfig config = {};
    ok &= expect(battery->get_charge_config(config), "battery gets charge config");
    ok &= expect(config.enabled, "battery charging is enabled by default");
    ok &= expect(battery->set_charging_enabled(false), "battery disables charging");
    ok &= expect(battery->get_charge_config(config), "battery gets updated charge config");
    ok &= expect(!config.enabled, "battery charging is disabled");

    return ok;
}

bool test_storage()
{
    bool ok = true;
    auto kv = get_iface<hal::StorageKvIface>(hal::StorageSimulatorDevice::KV_IFACE_NAME, ok);
    auto fs = get_iface<hal::StorageFsIface>(hal::StorageSimulatorDevice::FS_IFACE_NAME, ok);
    if (!kv || !fs) {
        return false;
    }

    hal::StorageKvIface::KeyValueMap values = {
        {"flag", true},
        {"count", static_cast<int32_t>(42)},
        {"name", std::string("sim")},
    };
    ok &= expect(kv->set("test", values), "storage KV sets values");

    std::vector<hal::StorageKvIface::EntryInfo> entries;
    ok &= expect(kv->list("test", entries), "storage KV lists namespace");
    ok &= expect(entries.size() == 3, "storage KV entry count matches");

    hal::StorageKvIface::KeyValueMap read_values;
    ok &= expect(kv->get("test", {"flag", "missing"}, read_values), "storage KV gets selected values");
    ok &= expect(read_values.size() == 1, "storage KV skips missing key");
    ok &= expect(std::get<bool>(read_values["flag"]), "storage KV bool value matches");

    ok &= expect(kv->get("test", {}, read_values), "storage KV gets all values");
    ok &= expect(read_values.size() == 3, "storage KV get-all count matches");
    ok &= expect(kv->erase("test", {"flag"}), "storage KV erases one key");
    ok &= expect(kv->get("test", {}, read_values), "storage KV gets values after erase");
    ok &= expect(read_values.size() == 2, "storage KV count after single erase matches");
    ok &= expect(kv->erase("test", {}), "storage KV erases namespace");
    ok &= expect(kv->get("test", {}, read_values), "storage KV gets erased namespace");
    ok &= expect(read_values.empty(), "storage KV namespace is empty");

    const auto &fs_infos = fs->get_all_info();
    ok &= expect(fs_infos.size() == 2, "storage FS entry count matches");
    ok &= expect(fs_infos[0].fs_type == hal::StorageFsIface::FileSystemType::SPIFFS, "first FS type matches");
    ok &= expect(fs_infos[0].medium_type == hal::StorageFsIface::MediumType::Flash, "first FS medium matches");
    ok &= expect(std::string(fs_infos[0].mount_point) == "/spiffs", "first FS mount point matches");
    ok &= expect(fs_infos[1].fs_type == hal::StorageFsIface::FileSystemType::LittleFS, "second FS type matches");
    ok &= expect(fs_infos[1].medium_type == hal::StorageFsIface::MediumType::Flash, "second FS medium matches");
    ok &= expect(std::string(fs_infos[1].mount_point) == "/littlefs", "second FS mount point matches");

    return ok;
}

bool test_deinit_releases_interfaces()
{
    bool ok = true;
    hal::deinit_all_devices();

    ok &= expect(!hal::get_first_interface<hal::BoardInfoIface>().second, "board info interface is released");
    ok &= expect(!hal::get_first_interface<hal::AudioCodecPlayerIface>().second, "audio player interface is released");
    ok &= expect(
              !hal::get_first_interface<hal::AudioCodecRecorderIface>().second,
              "audio recorder interface is released"
          );
    ok &= expect(!hal::get_first_interface<hal::DisplayPanelIface>().second, "display panel interface is released");
    ok &= expect(!hal::get_first_interface<hal::DisplayTouchIface>().second, "display touch interface is released");
    ok &= expect(
              !hal::get_first_interface<hal::DisplayBacklightIface>().second,
              "display backlight interface is released"
          );
    ok &= expect(!hal::get_first_interface<hal::PowerBatteryIface>().second, "power battery interface is released");
    ok &= expect(!hal::get_first_interface<hal::StorageKvIface>().second, "storage KV interface is released");
    ok &= expect(!hal::get_first_interface<hal::StorageFsIface>().second, "storage FS interface is released");
    ok &= expect(!hal::get_first_interface<hal::WifiBasicIface>().second, "Wi-Fi basic interface is released");
    ok &= expect(!hal::get_first_interface<hal::WifiStaIface>().second, "Wi-Fi STA interface is released");
    ok &= expect(!hal::get_first_interface<hal::WifiSoftApIface>().second, "Wi-Fi SoftAP interface is released");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= expect(hal::get_device_by_device_name(hal::GeneralSimulatorDevice::DEVICE_NAME) != nullptr,
                 "general simulator device is registered");
    ok &= expect(hal::get_device_by_device_name(hal::AudioSimulatorDevice::DEVICE_NAME) != nullptr,
                 "audio simulator device is registered");
    ok &= expect(hal::get_device_by_device_name(hal::DisplaySimulatorDevice::DEVICE_NAME) != nullptr,
                 "display simulator device is registered");
    ok &= expect(hal::get_device_by_device_name(hal::PowerSimulatorDevice::DEVICE_NAME) != nullptr,
                 "power simulator device is registered");
    ok &= expect(hal::get_device_by_device_name(hal::StorageSimulatorDevice::DEVICE_NAME) != nullptr,
                 "storage simulator device is registered");
    ok &= expect(hal::get_device_by_device_name(hal::WifiSimulatorDevice::DEVICE_NAME) != nullptr,
                 "Wi-Fi simulator device is registered");

    hal::init_all_devices();
    ok &= test_general();
    ok &= test_audio();
    ok &= test_display();
    ok &= test_power();
    ok &= test_storage();
    ok &= test_deinit_releases_interfaces();

    if (!ok) {
        return 1;
    }

    std::cout << "HAL simulator smoke test passed" << std::endl;
    return 0;
}
