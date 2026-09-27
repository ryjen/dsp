#pragma once

#include <dsp/targets/daisy/control_event.hpp>

#include <array>
#include <cstddef>

namespace dsp::targets::daisy {

class TempoController {
public:
    void on_midi_start(MonotonicMicros now) noexcept;
    void on_midi_continue(MonotonicMicros now) noexcept;
    void on_midi_stop() noexcept;
    void on_midi_clock(MonotonicMicros now) noexcept;
    void on_tap(MonotonicMicros now) noexcept;
    void update_timeout(MonotonicMicros now) noexcept;

    [[nodiscard]] float tempo_bpm() const noexcept;
    [[nodiscard]] bool midi_authoritative() const noexcept;

private:
    static constexpr std::size_t max_midi_intervals = 24;
    static constexpr std::size_t max_tap_intervals = 4;

    [[nodiscard]] bool clock_is_fresh(MonotonicMicros now) const noexcept;
    [[nodiscard]] MonotonicMicros clock_timeout() const noexcept;
    void publish_midi_estimate() noexcept;
    void publish_tap_estimate() noexcept;

    std::array<MonotonicMicros, max_midi_intervals> midi_intervals_{};
    std::size_t midi_interval_count_{0};
    std::size_t midi_interval_index_{0};
    MonotonicMicros last_clock_{0};
    bool has_last_clock_{false};
    bool has_midi_estimate_{false};
    bool midi_running_{false};
    bool midi_authoritative_{false};

    std::array<MonotonicMicros, max_tap_intervals> tap_intervals_{};
    std::size_t tap_interval_count_{0};
    std::size_t tap_interval_index_{0};
    MonotonicMicros last_tap_{0};
    bool has_last_tap_{false};

    float tempo_bpm_{120.0F};
};

}  // namespace dsp::targets::daisy
