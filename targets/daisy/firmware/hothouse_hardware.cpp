#include "hothouse_hardware.hpp"

#include <dsp/targets/daisy/hothouse_pins.hpp>

namespace dsp::targets::daisy::firmware {

HothouseHardware::HothouseHardware(::daisy::DaisySeed& seed) noexcept
    : seed_{seed} {}

void HothouseHardware::init() {
    adc_config_[0].InitSingle(hothouse::pot1);
    adc_config_[1].InitSingle(hothouse::pot2);
    seed_.adc.Init(adc_config_, 2);

    toggle1_up_.Init(
        hothouse::toggle1_up,
        ::daisy::GPIO::Mode::INPUT,
        ::daisy::GPIO::Pull::PULLUP);
    toggle1_down_.Init(
        hothouse::toggle1_down,
        ::daisy::GPIO::Mode::INPUT,
        ::daisy::GPIO::Pull::PULLUP);
    footswitch1_.Init(
        hothouse::footswitch1,
        ::daisy::GPIO::Mode::INPUT,
        ::daisy::GPIO::Pull::PULLUP);
    footswitch2_.Init(
        hothouse::footswitch2,
        ::daisy::GPIO::Mode::INPUT,
        ::daisy::GPIO::Pull::PULLUP);
}

void HothouseHardware::start_adc() {
    seed_.adc.Start();
}

float HothouseHardware::pot1() const noexcept {
    return seed_.adc.GetFloat(0);
}

float HothouseHardware::pot2() const noexcept {
    return seed_.adc.GetFloat(1);
}

std::uint8_t HothouseHardware::toggle1_position() noexcept {
    const bool up_active = !toggle1_up_.Read();
    const bool down_active = !toggle1_down_.Read();
    if (up_active && !down_active) return 0U;
    if (down_active && !up_active) return 2U;
    return 1U;
}

bool HothouseHardware::footswitch1_pressed() noexcept {
    return !footswitch1_.Read();
}

bool HothouseHardware::footswitch2_pressed() noexcept {
    return !footswitch2_.Read();
}

}  // namespace dsp::targets::daisy::firmware
