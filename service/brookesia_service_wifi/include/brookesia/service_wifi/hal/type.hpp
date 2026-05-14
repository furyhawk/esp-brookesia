/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service::wifi {

/**
 * @brief Re-exported helper types used by the Wi-Fi HAL-facing APIs.
 */
using GeneralAction = hal::WifiGeneralAction;
/**
 * @brief Re-exported general Wi-Fi event identifiers.
 */
using GeneralEvent = hal::WifiGeneralEvent;
/**
 * @brief Parameters used to connect to an access point.
 */
using ConnectApInfo = hal::WifiConnectApInfo;
/**
 * @brief Parameters used to configure an access-point scan.
 */
using ScanParams = hal::WifiScanParams;
/**
 * @brief Signal strength classification for scanned access points.
 */
using ScanApSignalLevel = hal::WifiScanApSignalLevel;
/**
 * @brief Information returned for one scanned access point.
 */
using ScanApInfo = hal::WifiScanApInfo;
/**
 * @brief Parameters used to configure soft-AP mode.
 */
using SoftApParams = hal::WifiSoftApParams;
/**
 * @brief Re-exported soft-AP event identifiers.
 */
using SoftApEvent = hal::WifiSoftApEvent;

} // namespace esp_brookesia::service::wifi
