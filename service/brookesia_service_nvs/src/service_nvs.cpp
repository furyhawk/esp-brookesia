/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <utility>
#include <vector>

#include "boost/format.hpp"
#include "brookesia/service_nvs/macro_configs.h"
#if !BROOKESIA_SERVICE_NVS_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_nvs/service_nvs.hpp"

namespace esp_brookesia::service {

namespace {

using StorageKvIface = hal::StorageKvIface;

std::vector<std::string> parse_keys(const boost::json::array &keys)
{
    std::vector<std::string> result;
    result.reserve(keys.size());
    for (const auto &key_json : keys) {
        if (!key_json.is_string()) {
            BROOKESIA_LOGW("Key must be a string, skipping");
            continue;
        }
        result.emplace_back(std::string(key_json.as_string()));
    }
    return result;
}

std::string get_last_error_or_default(const StorageKvIface &iface, const std::string &default_error)
{
    const auto &last_error = iface.get_last_error();
    if (!last_error.empty()) {
        return last_error;
    }
    return default_error;
}

} // namespace

bool NVS::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_NVS_VER_MAJOR, BROOKESIA_SERVICE_NVS_VER_MINOR,
        BROOKESIA_SERVICE_NVS_VER_PATCH
    );

    auto [kv_iface_name, kv_iface] = hal::get_first_interface<StorageKvIface>();
    BROOKESIA_CHECK_NULL_RETURN(kv_iface, false, "Storage KV HAL interface is not available");

    kv_iface_ = std::move(kv_iface);
    BROOKESIA_LOGI("Using storage KV interface: %1%", kv_iface_name);

    if (!kv_iface_->init()) {
        BROOKESIA_LOGE("Failed to initialize storage KV interface: %1%", kv_iface_->get_last_error());
        kv_iface_.reset();
        return false;
    }

    return true;
}

void NVS::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (kv_iface_) {
        kv_iface_->deinit();
        kv_iface_.reset();
    }
}

std::expected<boost::json::array, std::string> NVS::function_list(const std::string &nspace)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%)", nspace);

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    std::vector<StorageKvIface::EntryInfo> entries;
    if (!kv_iface_->list(nspace, entries)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to list entries in namespace '%1%'"
                               ) % nspace).str()));
    }

    return BROOKESIA_DESCRIBE_TO_JSON(entries).as_array();
}

std::expected<void, std::string> NVS::function_set(const std::string &nspace, boost::json::object &&key_value_map)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), key_value_map(%2%)", nspace, BROOKESIA_DESCRIBE_TO_STR(key_value_map));

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    KeyValueMap parsed_key_value_map;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(key_value_map, parsed_key_value_map)) {
        return std::unexpected((
                                   boost::format("Failed to parse key-value map in namespace '%1%': %2%") %
                                   nspace % BROOKESIA_DESCRIBE_TO_STR(key_value_map)
                               ).str());
    }

    if (!kv_iface_->set(nspace, parsed_key_value_map)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to set key-value map in namespace '%1%'"
                               ) % nspace).str()));
    }

    return {};
}

std::expected<boost::json::object, std::string> NVS::function_get(const std::string &nspace, boost::json::array &&keys)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, keys);

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    auto parsed_keys = parse_keys(keys);
    if (!keys.empty() && parsed_keys.empty()) {
        return boost::json::object();
    }

    KeyValueMap key_value_map;
    if (!kv_iface_->get(nspace, parsed_keys, key_value_map)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to get key-value map in namespace '%1%'"
                               ) % nspace).str()));
    }

    return BROOKESIA_DESCRIBE_TO_JSON(key_value_map).as_object();
}

std::expected<void, std::string> NVS::function_erase(const std::string &nspace, boost::json::array &&keys)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: nspace(%1%), keys(%2%)", nspace, keys);

    if (!kv_iface_) {
        return std::unexpected("Storage KV HAL interface is not available");
    }

    auto parsed_keys = parse_keys(keys);
    if (!keys.empty() && parsed_keys.empty()) {
        return {};
    }

    if (!kv_iface_->erase(nspace, parsed_keys)) {
        return std::unexpected(get_last_error_or_default(*kv_iface_, (boost::format(
                                   "Failed to erase key-value map in namespace '%1%'"
                               ) % nspace).str()));
    }

    return {};
}

#if BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, NVS, NVS::get_instance().get_attributes().name, NVS::get_instance(),
    BROOKESIA_SERVICE_NVS_PLUGIN_SYMBOL
);
#endif // BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
