/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file kv.hpp
 * @brief Declares the generic key-value storage HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Generic namespace-based key-value storage interface.
 */
class StorageKvIface: public Interface {
public:
    static constexpr const char *NAME = "StorageKv";  ///< Interface registry name.

    /**
     * @brief Supported stored value type.
     */
    enum class ValueType {
        Bool,   ///< Boolean value.
        Int,    ///< 32-bit signed integer value.
        String, ///< String value.
        Max,    ///< Invalid value type.
    };

    /**
     * @brief Value stored in a key-value namespace.
     */
    using Value = std::variant<bool, int32_t, std::string>;

    /**
     * @brief Key-value mapping payload for batch operations.
     */
    using KeyValueMap = std::map<std::string, Value>;

    /**
     * @brief Metadata for one stored entry.
     */
    struct EntryInfo {
        std::string nspace; ///< Namespace name.
        std::string key;    ///< Entry key.
        ValueType type;     ///< Entry value type.
    };

    /**
     * @brief Construct a key-value storage interface.
     */
    StorageKvIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic storage interfaces.
     */
    virtual ~StorageKvIface() = default;

    /**
     * @brief Initialize the key-value storage backend.
     *
     * @return true on success, otherwise false.
     */
    virtual bool init() = 0;

    /**
     * @brief Deinitialize the key-value storage backend.
     */
    virtual void deinit() = 0;

    /**
     * @brief List entries under a namespace.
     *
     * @param[in] nspace Namespace to list.
     * @param[out] entries Listed entries.
     * @return true on success, otherwise false.
     */
    virtual bool list(const std::string &nspace, std::vector<EntryInfo> &entries) = 0;

    /**
     * @brief Set key-value pairs under a namespace.
     *
     * @param[in] nspace Namespace to write.
     * @param[in] key_value_map Values to write.
     * @return true on success, otherwise false.
     */
    virtual bool set(const std::string &nspace, const KeyValueMap &key_value_map) = 0;

    /**
     * @brief Get key-value pairs under a namespace.
     *
     * Passing an empty key list returns all entries in the namespace.
     *
     * @param[in] nspace Namespace to read.
     * @param[in] keys Keys to read. Empty means all keys.
     * @param[out] key_value_map Read values.
     * @return true on success, otherwise false.
     */
    virtual bool get(
        const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map
    ) = 0;

    /**
     * @brief Erase keys under a namespace.
     *
     * Passing an empty key list erases the whole namespace.
     *
     * @param[in] nspace Namespace to erase.
     * @param[in] keys Keys to erase. Empty means all keys.
     * @return true on success, otherwise false.
     */
    virtual bool erase(const std::string &nspace, const std::vector<std::string> &keys) = 0;

    /**
     * @brief Get the most recent backend error.
     *
     * @return Human-readable error string.
     */
    const std::string &get_last_error() const
    {
        return last_error_;
    }

protected:
    std::string last_error_;
};

BROOKESIA_DESCRIBE_ENUM(StorageKvIface::ValueType, Bool, Int, String, Max);
BROOKESIA_DESCRIBE_STRUCT(StorageKvIface::EntryInfo, (), (nspace, key, type));

} // namespace esp_brookesia::hal
