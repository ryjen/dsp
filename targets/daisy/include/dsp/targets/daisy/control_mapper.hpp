#pragma once

#include <dsp/control/pedal_control_state.hpp>
#include <dsp/targets/daisy/control_event.hpp>
#include <dsp/targets/daisy/tempo_controller.hpp>

#include <cstdint>

namespace dsp::targets::daisy {

class ControlMapper {
public:
    ControlMapper(
        dsp::control::PedalControlState& state,
        TempoController& tempo) noexcept;

    void update_pot1(float normalized) noexcept;
    void update_pot2(float normalized) noexcept;
    void update_toggle1(std::uint8_t position, MonotonicMicros now) noexcept;
    void update_footswitch1(bool pressed, MonotonicMicros now) noexcept;
    void update_footswitch2(bool pressed, MonotonicMicros now) noexcept;
    void apply_midi_control(std::uint8_t cc, float normalized) noexcept;

    [[nodiscard]] dsp::control::PedalControlSnapshot snapshot() const noexcept;

private:
    template <typename T>
    struct DebouncedInput {
        T stable{};
        T candidate{};
        MonotonicMicros candidate_since{0};
        bool pending{false};

        bool update(T raw, MonotonicMicros now) noexcept {
            if (raw == stable) {
                pending = false;
                return false;
            }
            if (!pending || raw != candidate) {
                candidate = raw;
                candidate_since = now;
                pending = true;
                return false;
            }
            const MonotonicMicros elapsed =
                elapsed_micros(now, candidate_since);
            if (!valid_forward_elapsed(elapsed) ||
                elapsed < debounce_micros) {
                return false;
            }
            stable = candidate;
            pending = false;
            return true;
        }
    };

    static constexpr MonotonicMicros debounce_micros = 20000;
    static constexpr float pot_deadband = 0.002F;

    void publish_primary(float normalized) noexcept;
    void publish_secondary(float normalized) noexcept;
    void publish_mode(dsp::control::PedalMode mode) noexcept;
    void publish_bypass_toggle() noexcept;
    void publish_tempo() noexcept;

    dsp::control::PedalControlState& state_;
    TempoController& tempo_;
    float last_physical_primary_{0.0F};
    float last_physical_secondary_{0.0F};
    DebouncedInput<std::uint8_t> toggle1_{};
    DebouncedInput<bool> footswitch1_{};
    DebouncedInput<bool> footswitch2_{};
};

}  // namespace dsp::targets::daisy
