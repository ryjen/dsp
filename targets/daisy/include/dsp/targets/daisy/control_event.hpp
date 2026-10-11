#pragma once

#include <cstdint>

namespace dsp::targets::daisy {

using MonotonicMicros = std::uint32_t;

inline constexpr MonotonicMicros half_timestamp_range =
    MonotonicMicros{1U} << 31U;

[[nodiscard]] constexpr MonotonicMicros elapsed_micros(
    MonotonicMicros now,
    MonotonicMicros earlier) noexcept {
    return now - earlier;
}

[[nodiscard]] constexpr bool valid_forward_elapsed(
    MonotonicMicros elapsed) noexcept {
    return elapsed != 0U && elapsed < half_timestamp_range;
}

enum class ControlEventKind : std::uint8_t {
    midi_clock,
    transport_start,
    transport_stop,
    transport_continue,
    primary_normalized,
    secondary_normalized,
};

struct ControlEvent {
    ControlEventKind kind{ControlEventKind::midi_clock};
    float normalized{0.0F};
    MonotonicMicros timestamp{0};
};

}  // namespace dsp::targets::daisy
