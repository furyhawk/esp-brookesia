/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cstddef>
#include <mutex>

#include "brookesia/hal_simulator/macro_configs.h"
#if !BROOKESIA_HAL_SIMULATOR_AUDIO_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_simulator/audio/device.hpp"

namespace esp_brookesia::hal {

namespace {

AudioCodecRecorderIface::Info make_recorder_info()
{
    return AudioCodecRecorderIface::Info{
        .bits = 16,
        .channels = 2,
        .sample_rate = 16000,
        .mic_layout = "LR",
        .general_gain = 1.0f,
        .channel_gains = {
            {0, 1.0f},
            {1, 1.0f},
        },
    };
}

} // namespace

class AudioCodecPlayerSimulatorImpl: public AudioCodecPlayerIface {
public:
    bool open(const Config &config) override
    {
        std::lock_guard lock(mutex_);
        if ((config.bits == 0) || (config.channels == 0) || (config.sample_rate == 0)) {
            BROOKESIA_LOGE(
                "Invalid player config: bits(%1%), channels(%2%), sample_rate(%3%)",
                config.bits, config.channels, config.sample_rate
            );
            return false;
        }

        config_ = config;
        is_opened_ = true;
        total_bytes_written_ = 0;
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = false;
    }

    bool set_volume(uint8_t volume) override
    {
        std::lock_guard lock(mutex_);
        volume_ = volume;
        return true;
    }

    bool write_data(const uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        if (data == nullptr) {
            BROOKESIA_LOGE("Player write data pointer is null");
            return false;
        }

        total_bytes_written_ += size;
        return true;
    }

    bool is_pa_on_off_supported() override
    {
        return true;
    }

    bool set_pa_on_off(bool on) override
    {
        std::lock_guard lock(mutex_);
        pa_on_ = on;
        return true;
    }

    bool is_pa_on() const override
    {
        std::lock_guard lock(mutex_);
        return pa_on_;
    }

private:
    mutable std::mutex mutex_;
    Config config_{};
    bool is_opened_ = false;
    bool pa_on_ = true;
    uint8_t volume_ = 0;
    size_t total_bytes_written_ = 0;
};

class AudioCodecRecorderSimulatorImpl: public AudioCodecRecorderIface {
public:
    AudioCodecRecorderSimulatorImpl()
        : AudioCodecRecorderIface(make_recorder_info())
    {
    }

    bool open() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = true;
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = false;
    }

    bool read_data(uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        if (data == nullptr) {
            BROOKESIA_LOGE("Recorder read data pointer is null");
            return false;
        }

        std::fill_n(data, size, static_cast<uint8_t>(0x5A));
        return true;
    }

    bool set_general_gain(float gain) override
    {
        std::lock_guard lock(mutex_);
        const_cast<Info &>(get_info()).general_gain = gain;
        return true;
    }

    bool set_channel_gains(const std::map<uint8_t, float> &gains) override
    {
        std::lock_guard lock(mutex_);
        const_cast<Info &>(get_info()).channel_gains = gains;
        return true;
    }

private:
    std::mutex mutex_;
    bool is_opened_ = false;
};

bool AudioSimulatorDevice::probe()
{
    return true;
}

bool AudioSimulatorDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        player_iface_ = std::make_shared<AudioCodecPlayerSimulatorImpl>(), false,
        "Failed to create audio player simulator"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        recorder_iface_ = std::make_shared<AudioCodecRecorderSimulatorImpl>(), false,
        "Failed to create audio recorder simulator"
    );

    interfaces_.emplace(PLAYER_IFACE_NAME, player_iface_);
    interfaces_.emplace(RECORDER_IFACE_NAME, recorder_iface_);

    return true;
}

void AudioSimulatorDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(RECORDER_IFACE_NAME);
    interfaces_.erase(PLAYER_IFACE_NAME);
    recorder_iface_.reset();
    player_iface_.reset();
}

#if BROOKESIA_HAL_SIMULATOR_ENABLE_AUDIO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, AudioSimulatorDevice, AudioSimulatorDevice::DEVICE_NAME, AudioSimulatorDevice::get_instance(),
    BROOKESIA_HAL_SIMULATOR_AUDIO_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
