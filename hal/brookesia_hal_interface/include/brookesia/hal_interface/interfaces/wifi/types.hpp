/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"

/**
 * @file types.hpp
 * @brief Declares shared Wi-Fi HAL interface types.
 */

namespace esp_brookesia::hal {

/**
 * @brief General Wi-Fi control action used by service-level dispatch.
 */
enum class WifiGeneralAction {
    Init,       ///< Initialize Wi-Fi resources.
    Deinit,     ///< Deinitialize Wi-Fi resources.
    Start,      ///< Start Wi-Fi subsystem.
    Stop,       ///< Stop Wi-Fi subsystem.
    Connect,    ///< Connect to configured AP.
    Disconnect, ///< Disconnect from current AP.
    Max,        ///< Sentinel value.
};

/**
 * @brief General Wi-Fi lifecycle event used by service-level dispatch.
 */
enum class WifiGeneralEvent {
    Deinited,     ///< Wi-Fi has been deinitialized.
    Inited,       ///< Wi-Fi has been initialized.
    Stopped,      ///< Wi-Fi has been stopped.
    Started,      ///< Wi-Fi has started.
    Disconnected, ///< Wi-Fi disconnected from AP.
    Connected,    ///< Wi-Fi connected to AP.
    Max,          ///< Sentinel value.
};

/**
 * @brief Basic Wi-Fi controller lifecycle action.
 */
enum class WifiBasicAction {
    Init,   ///< Initialize Wi-Fi resources.
    Deinit, ///< Deinitialize Wi-Fi resources.
    Start,  ///< Start Wi-Fi subsystem.
    Stop,   ///< Stop Wi-Fi subsystem.
    Max,    ///< Sentinel value.
};

/**
 * @brief Basic Wi-Fi controller lifecycle event.
 */
enum class WifiBasicEvent {
    Deinited, ///< Wi-Fi has been deinitialized.
    Inited,   ///< Wi-Fi has been initialized.
    Stopped,  ///< Wi-Fi has been stopped.
    Started,  ///< Wi-Fi has started.
    Max,      ///< Sentinel value.
};

/**
 * @brief Wi-Fi station action.
 */
enum class WifiStaAction {
    Connect,    ///< Connect to configured AP.
    Disconnect, ///< Disconnect from current AP.
    Max,        ///< Sentinel value.
};

/**
 * @brief Wi-Fi station event.
 */
enum class WifiStaEvent {
    Disconnected, ///< Wi-Fi disconnected from AP.
    Connected,    ///< Wi-Fi connected to AP.
    Max,          ///< Sentinel value.
};

/**
 * @brief Target AP connection information.
 */
struct WifiConnectApInfo {
    WifiConnectApInfo() = default;
    WifiConnectApInfo(const std::string &ssid, const std::string &password = "")
        : ssid(ssid)
        , password(password)
    {
    }

    bool operator==(const WifiConnectApInfo &other) const
    {
        return (ssid == other.ssid) && (password == other.password) && (is_connectable == other.is_connectable);
    }

    bool operator!=(const WifiConnectApInfo &other) const
    {
        return !(*this == other);
    }

    bool is_valid() const
    {
        return !ssid.empty();
    }

    std::string ssid;             ///< AP SSID.
    std::string password;         ///< AP password.
    bool is_connectable = true;   ///< Whether this AP is considered connectable.
};

/**
 * @brief Parameters for periodic AP scanning.
 */
struct WifiScanParams {
    bool operator==(const WifiScanParams &other) const
    {
        return (ap_count == other.ap_count) && (interval_ms == other.interval_ms) && (timeout_ms == other.timeout_ms);
    }

    bool operator!=(const WifiScanParams &other) const
    {
        return !(*this == other);
    }

    size_t ap_count = 20;         ///< Maximum number of AP entries to keep.
    uint32_t interval_ms = 10000; ///< Scan interval in milliseconds.
    uint32_t timeout_ms = 60000;  ///< Total scan timeout in milliseconds.
};

/**
 * @brief Signal-strength bucket for a scanned AP.
 */
enum class WifiScanApSignalLevel {
    LEVEL_0, ///< RSSI <= -81 dBm.
    LEVEL_1, ///< RSSI in [-80, -71] dBm.
    LEVEL_2, ///< RSSI in [-70, -61] dBm.
    LEVEL_3, ///< RSSI in [-60, -51] dBm.
    LEVEL_4, ///< RSSI >= -50 dBm.
};

/**
 * @brief One scanned AP entry.
 */
struct WifiScanApInfo {
    static WifiScanApSignalLevel get_signal_level(int rssi)
    {
        if (rssi <= -81) {
            return WifiScanApSignalLevel::LEVEL_0;
        }
        if (rssi <= -71) {
            return WifiScanApSignalLevel::LEVEL_1;
        }
        if (rssi <= -61) {
            return WifiScanApSignalLevel::LEVEL_2;
        }
        if (rssi <= -51) {
            return WifiScanApSignalLevel::LEVEL_3;
        }
        return WifiScanApSignalLevel::LEVEL_4;
    }

    std::string ssid;                         ///< AP SSID.
    bool is_locked = false;                   ///< Whether AP authentication is required.
    int rssi = 0;                             ///< Signal strength in dBm.
    WifiScanApSignalLevel signal_level = WifiScanApSignalLevel::LEVEL_0; ///< Bucketed signal level.
    uint8_t channel = 0;                      ///< RF channel.
};

/**
 * @brief SoftAP runtime configuration.
 */
struct WifiSoftApParams {
    bool operator==(const WifiSoftApParams &other) const
    {
        if ((ssid != other.ssid) || (password != other.password) ||
                (max_connection != other.max_connection) || (channel.has_value() != other.channel.has_value())) {
            return false;
        }
        if (channel.has_value() && (get_channel() != other.get_channel())) {
            return false;
        }
        return true;
    }

    bool operator!=(const WifiSoftApParams &other) const
    {
        return !(*this == other);
    }

    bool has_channel() const
    {
        return channel.has_value();
    }

    uint8_t get_channel() const
    {
        return channel.value();
    }

    std::string ssid;                         ///< SoftAP SSID.
    std::string password;                     ///< SoftAP password.
    uint8_t max_connection = 4;               ///< Maximum station connections.
    std::optional<uint8_t> channel = std::nullopt; ///< Optional fixed channel.
};

/**
 * @brief SoftAP lifecycle event.
 */
enum class WifiSoftApEvent {
    Started, ///< SoftAP started.
    Stopped, ///< SoftAP stopped.
    Max,     ///< Sentinel value.
};

using WifiConnectApInfoList = std::list<WifiConnectApInfo>;
using WifiScanApInfoList = std::vector<WifiScanApInfo>;

BROOKESIA_DESCRIBE_ENUM(WifiGeneralAction, Init, Deinit, Start, Stop, Connect, Disconnect, Max);
BROOKESIA_DESCRIBE_ENUM(WifiGeneralEvent, Deinited, Inited, Stopped, Started, Disconnected, Connected, Max);
BROOKESIA_DESCRIBE_ENUM(WifiBasicAction, Init, Deinit, Start, Stop, Max);
BROOKESIA_DESCRIBE_ENUM(WifiBasicEvent, Deinited, Inited, Stopped, Started, Max);
BROOKESIA_DESCRIBE_ENUM(WifiStaAction, Connect, Disconnect, Max);
BROOKESIA_DESCRIBE_ENUM(WifiStaEvent, Disconnected, Connected, Max);
BROOKESIA_DESCRIBE_STRUCT(WifiConnectApInfo, (), (ssid, password, is_connectable));
BROOKESIA_DESCRIBE_STRUCT(WifiScanParams, (), (ap_count, interval_ms, timeout_ms));
BROOKESIA_DESCRIBE_ENUM(WifiScanApSignalLevel, LEVEL_0, LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4);
BROOKESIA_DESCRIBE_STRUCT(WifiScanApInfo, (), (ssid, is_locked, rssi, signal_level, channel));
BROOKESIA_DESCRIBE_STRUCT(WifiSoftApParams, (), (ssid, password, max_connection, channel));
BROOKESIA_DESCRIBE_ENUM(WifiSoftApEvent, Started, Stopped, Max);

} // namespace esp_brookesia::hal
