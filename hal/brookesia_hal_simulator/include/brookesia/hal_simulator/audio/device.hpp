/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class AudioCodecPlayerSimulatorImpl;
class AudioCodecRecorderSimulatorImpl;

class AudioSimulatorDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "AudioSimulator";
    static constexpr const char *PLAYER_IFACE_NAME = "AudioSimulator:Player";
    static constexpr const char *RECORDER_IFACE_NAME = "AudioSimulator:Recorder";

    AudioSimulatorDevice(const AudioSimulatorDevice &) = delete;
    AudioSimulatorDevice &operator=(const AudioSimulatorDevice &) = delete;
    AudioSimulatorDevice(AudioSimulatorDevice &&) = delete;
    AudioSimulatorDevice &operator=(AudioSimulatorDevice &&) = delete;

    static AudioSimulatorDevice &get_instance()
    {
        static AudioSimulatorDevice instance;
        return instance;
    }

private:
    AudioSimulatorDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~AudioSimulatorDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<AudioCodecPlayerSimulatorImpl> player_iface_;
    std::shared_ptr<AudioCodecRecorderSimulatorImpl> recorder_iface_;
};

} // namespace esp_brookesia::hal
