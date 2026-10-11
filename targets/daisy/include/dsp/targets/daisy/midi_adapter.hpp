#pragma once

#include <dsp/targets/daisy/control_event.hpp>

#include <cstdint>
#include <optional>

namespace dsp::targets::daisy {

enum class MidiMessageKind : std::uint8_t {
    clock,
    start,
    stop,
    continue_playback,
    control_change,
    program_change,
    unsupported,
};

struct MidiMessage {
    MidiMessageKind kind{MidiMessageKind::unsupported};
    std::uint8_t channel{0};
    std::uint8_t data1{0};
    std::uint8_t data2{0};
    bool valid{false};
};

class MidiAdapter {
public:
    [[nodiscard]] std::optional<ControlEvent> translate(
        const MidiMessage& message,
        MonotonicMicros timestamp) const noexcept;
};

}  // namespace dsp::targets::daisy
