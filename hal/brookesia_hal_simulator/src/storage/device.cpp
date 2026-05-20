/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <map>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

#include "brookesia/hal_simulator/macro_configs.h"
#if !BROOKESIA_HAL_SIMULATOR_STORAGE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/storage/fs.hpp"
#include "brookesia/hal_interface/interfaces/storage/kv.hpp"
#include "brookesia/hal_simulator/storage/device.hpp"

namespace esp_brookesia::hal {

class StorageKvSimulatorImpl: public StorageKvIface {
public:
    bool init() override
    {
        std::lock_guard lock(mutex_);
        is_initialized_ = true;
        last_error_.clear();
        return true;
    }

    void deinit() override
    {
        std::lock_guard lock(mutex_);
        namespaces_.clear();
        is_initialized_ = false;
        last_error_.clear();
    }

    bool list(const std::string &nspace, std::vector<EntryInfo> &entries) override
    {
        std::lock_guard lock(mutex_);
        entries.clear();
        if (!check_initialized()) {
            return false;
        }

        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }

        entries.reserve(namespace_it->second.size());
        for (const auto &[key, value] : namespace_it->second) {
            entries.push_back(EntryInfo{
                .nspace = nspace,
                .key = key,
                .type = get_value_type(value),
            });
        }

        last_error_.clear();
        return true;
    }

    bool set(const std::string &nspace, const KeyValueMap &key_value_map) override
    {
        std::lock_guard lock(mutex_);
        if (!check_initialized()) {
            return false;
        }

        auto &storage = namespaces_[nspace];
        for (const auto &[key, value] : key_value_map) {
            storage[key] = value;
        }

        last_error_.clear();
        return true;
    }

    bool get(const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map) override
    {
        std::lock_guard lock(mutex_);
        key_value_map.clear();
        if (!check_initialized()) {
            return false;
        }

        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }

        if (keys.empty()) {
            key_value_map = namespace_it->second;
            last_error_.clear();
            return true;
        }

        for (const auto &key : keys) {
            auto value_it = namespace_it->second.find(key);
            if (value_it != namespace_it->second.end()) {
                key_value_map.emplace(key, value_it->second);
            }
        }

        last_error_.clear();
        return true;
    }

    bool erase(const std::string &nspace, const std::vector<std::string> &keys) override
    {
        std::lock_guard lock(mutex_);
        if (!check_initialized()) {
            return false;
        }

        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }

        if (keys.empty()) {
            namespaces_.erase(namespace_it);
            last_error_.clear();
            return true;
        }

        for (const auto &key : keys) {
            namespace_it->second.erase(key);
        }
        if (namespace_it->second.empty()) {
            namespaces_.erase(namespace_it);
        }

        last_error_.clear();
        return true;
    }

private:
    static ValueType get_value_type(const Value &value)
    {
        return std::visit([](auto &&stored_value) {
            using T = std::decay_t<decltype(stored_value)>;
            if constexpr (std::is_same_v<T, bool>) {
                return ValueType::Bool;
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return ValueType::Int;
            } else {
                return ValueType::String;
            }
        }, value);
    }

    bool check_initialized()
    {
        if (is_initialized_) {
            return true;
        }

        last_error_ = "Storage KV simulator is not initialized";
        BROOKESIA_LOGE("%1%", last_error_);
        return false;
    }

    bool is_initialized_ = false;
    std::mutex mutex_;
    std::map<std::string, KeyValueMap> namespaces_;
};

class StorageFsSimulatorImpl: public StorageFsIface {
public:
    StorageFsSimulatorImpl()
        : StorageFsIface()
    {
        info_list_ = {
            Info{
                .fs_type = FileSystemType::SPIFFS,
                .medium_type = MediumType::Flash,
                .mount_point = "/spiffs",
                .supports_directories = false,
            },
            Info{
                .fs_type = FileSystemType::LittleFS,
                .medium_type = MediumType::Flash,
                .mount_point = "/littlefs",
                .supports_directories = true,
            },
        };
    }

    bool get_capacity(const char *mount_point, Capacity &capacity) override
    {
        if (std::string(mount_point) == "/spiffs") {
            capacity = {
                .total_bytes = 1024 * 1024,
                .used_bytes = 128 * 1024,
                .free_bytes = 896 * 1024,
            };
            return true;
        }
        if (std::string(mount_point) == "/littlefs") {
            capacity = {
                .total_bytes = 2 * 1024 * 1024,
                .used_bytes = 256 * 1024,
                .free_bytes = 1792 * 1024,
            };
            return true;
        }

        return false;
    }
};

bool StorageSimulatorDevice::probe()
{
    return true;
}

bool StorageSimulatorDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        kv_iface_ = std::make_shared<StorageKvSimulatorImpl>(), false, "Failed to create storage KV simulator"
    );
    BROOKESIA_CHECK_FALSE_RETURN(kv_iface_->init(), false, "Failed to initialize storage KV simulator");
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        fs_iface_ = std::make_shared<StorageFsSimulatorImpl>(), false, "Failed to create storage FS simulator"
    );

    interfaces_.emplace(KV_IFACE_NAME, kv_iface_);
    interfaces_.emplace(FS_IFACE_NAME, fs_iface_);
    return true;
}

void StorageSimulatorDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (kv_iface_) {
        kv_iface_->deinit();
    }
    interfaces_.erase(FS_IFACE_NAME);
    interfaces_.erase(KV_IFACE_NAME);
    fs_iface_.reset();
    kv_iface_.reset();
}

#if BROOKESIA_HAL_SIMULATOR_ENABLE_STORAGE_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, StorageSimulatorDevice, StorageSimulatorDevice::DEVICE_NAME, StorageSimulatorDevice::get_instance(),
    BROOKESIA_HAL_SIMULATOR_STORAGE_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
