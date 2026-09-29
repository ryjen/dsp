#pragma once

#include <cstdint>

namespace dsp::targets::daisy {

using MonotonicMicros = std::uint64_t;

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
