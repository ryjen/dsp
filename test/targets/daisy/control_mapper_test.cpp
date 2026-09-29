#include <dsp/control/pedal_control_state.hpp>
#include <dsp/effects/delay.hpp>
#include <dsp/targets/daisy/control_mapper.hpp>
#include <dsp/targets/daisy/delay_control_binding.hpp>
#include <dsp/targets/daisy/tempo_controller.hpp>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};

bool near(float actual, float expected, float tolerance = 1.0e-4F) {
    return std::abs(actual - expected) <= tolerance;
}
}

void* operator new(std::size_t size) {
    if (tracking.load(std::memory_order_relaxed)) {
        allocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

int main() {
    using dsp::control::PedalControlState;
    using dsp::control::PedalMode;
    using dsp::effects::DelayProcessor;
    using dsp::effects::DelaySubdivision;
    using dsp::targets::daisy::ControlMapper;
    using dsp::targets::daisy::DelayControlBinding;
    using dsp::targets::daisy::TempoController;

    PedalControlState state;
    TempoController tempo;
    ControlMapper mapper{state, tempo};

    mapper.update_pot1(0.001F);
    if (mapper.snapshot().primary_normalized != 0.0F) return 1;
    mapper.update_pot1(0.002F);
    if (mapper.snapshot().primary_normalized != 0.002F) return 2;
    mapper.update_pot1(1.0F);
    mapper.update_pot2(1.0F);

    DelayProcessor delay;
    DelayControlBinding::apply(delay, mapper.snapshot());
    if (!near(delay.delay_ms(), 2000.0F)) return 3;
    if (!near(delay.feedback(), 0.95F)) return 4;

    PedalControlState physical_state;
    TempoController physical_tempo;
    ControlMapper physical{physical_state, physical_tempo};
    physical.update_pot1(0.5F);
    physical.update_pot2(0.25F);
    DelayProcessor physical_delay;
    DelayControlBinding::apply(physical_delay, physical.snapshot());

    PedalControlState midi_state;
    TempoController midi_tempo;
    ControlMapper midi{midi_state, midi_tempo};
    midi.apply_midi_control(20, 0.5F);
    midi.apply_midi_control(21, 0.25F);
    DelayProcessor midi_delay;
    DelayControlBinding::apply(midi_delay, midi.snapshot());
    if (!near(physical_delay.delay_ms(), midi_delay.delay_ms())) return 5;
    if (!near(physical_delay.feedback(), midi_delay.feedback())) return 6;

    const float expected_mid_delay = std::exp(std::log(2000.0F) * 0.5F);
    if (!near(physical_delay.delay_ms(), expected_mid_delay)) return 7;
    if (!near(physical_delay.feedback(), 0.95F * 0.25F)) return 8;

    const auto before_invalid = mapper.snapshot();
    mapper.update_pot1(std::numeric_limits<float>::quiet_NaN());
    mapper.update_pot1(-0.1F);
    mapper.update_pot2(1.1F);
    mapper.apply_midi_control(19, 0.8F);
    const auto after_invalid = mapper.snapshot();
    if (after_invalid.primary_normalized != before_invalid.primary_normalized) return 9;
    if (after_invalid.secondary_normalized != before_invalid.secondary_normalized) return 10;

    mapper.update_toggle1(1, 100000);
    if (mapper.snapshot().mode != PedalMode::free) return 11;
    mapper.update_toggle1(1, 119999);
    if (mapper.snapshot().mode != PedalMode::free) return 12;
    mapper.update_toggle1(1, 120000);
    if (mapper.snapshot().mode != PedalMode::quarter) return 13;

    const auto before_bad_toggle = mapper.snapshot().mode;
    mapper.update_toggle1(3, 140000);
    mapper.update_toggle1(3, 180000);
    if (mapper.snapshot().mode != before_bad_toggle) return 14;

    mapper.update_footswitch1(true, 200000);
    mapper.update_footswitch1(true, 219999);
    if (mapper.snapshot().bypassed) return 15;
    mapper.update_footswitch1(true, 220000);
    if (!mapper.snapshot().bypassed) return 16;
    mapper.update_footswitch1(false, 230000);
    mapper.update_footswitch1(false, 250000);
    mapper.update_footswitch1(true, 260000);
    mapper.update_footswitch1(true, 279999);
    if (!mapper.snapshot().bypassed) return 17;
    mapper.update_footswitch1(true, 280000);
    if (mapper.snapshot().bypassed) return 18;

    PedalControlState tap_state;
    TempoController tap_tempo;
    ControlMapper taps{tap_state, tap_tempo};
    taps.update_footswitch2(true, 1000000);
    taps.update_footswitch2(true, 1020000);
    taps.update_footswitch2(false, 1030000);
    taps.update_footswitch2(false, 1050000);
    taps.update_footswitch2(true, 1500000);
    taps.update_footswitch2(true, 1520000);
    if (!near(taps.snapshot().tempo_bpm, 120.0F, 0.1F)) return 19;
    taps.update_footswitch2(false, 1530000);
    taps.update_footswitch2(false, 1550000);
    taps.update_footswitch2(true, 2100000);
    taps.update_footswitch2(true, 2120000);
    if (!near(taps.snapshot().tempo_bpm, 109.0909F, 0.1F)) return 20;

    DelayControlBinding::apply(delay, {0.5F, 0.25F, 100.0F, PedalMode::quarter, false, true});
    if (delay.subdivision() != DelaySubdivision::quarter) return 21;
    if (!near(delay.tempo_bpm(), 100.0F)) return 22;

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000; ++i) {
        const float value = static_cast<float>(i % 100U) / 100.0F;
        mapper.update_pot1(value);
        mapper.update_pot2(1.0F - value);
        mapper.apply_midi_control(20, value);
        mapper.apply_midi_control(21, 1.0F - value);
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 23;

    return 0;
}
