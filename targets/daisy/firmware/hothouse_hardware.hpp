#pragma once

#include "daisy_seed.h"
#include "per/adc.h"
#include "per/gpio.h"

#include <cstdint>

namespace dsp::targets::daisy::firmware {

class HothouseHardware {
public:
    explicit HothouseHardware(::daisy::DaisySeed& seed) noexcept;

    void init();
    void start_adc();

    [[nodiscard]] float pot1() const noexcept;
    [[nodiscard]] float pot2() const noexcept;
    [[nodiscard]] std::uint8_t toggle1_position() noexcept;
    [[nodiscard]] bool footswitch1_pressed() noexcept;
    [[nodiscard]] bool footswitch2_pressed() noexcept;

private:
    ::daisy::DaisySeed& seed_;
    ::daisy::AdcChannelConfig adc_config_[2]{};
    ::daisy::GPIO toggle1_up_;
    ::daisy::GPIO toggle1_down_;
    ::daisy::GPIO footswitch1_;
    ::daisy::GPIO footswitch2_;
};

}  // namespace dsp::targets::daisy::firmware
