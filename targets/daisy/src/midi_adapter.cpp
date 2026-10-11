#include <dsp/targets/daisy/midi_adapter.hpp>

namespace dsp::targets::daisy {

std::optional<ControlEvent> MidiAdapter::translate(
    const MidiMessage& message,
    MonotonicMicros timestamp) const noexcept {
    if (!message.valid) return std::nullopt;

    switch (message.kind) {
        case MidiMessageKind::clock:
            return ControlEvent{ControlEventKind::midi_clock, 0.0F, timestamp};
        case MidiMessageKind::start:
            return ControlEvent{
                ControlEventKind::transport_start, 0.0F, timestamp};
        case MidiMessageKind::stop:
            return ControlEvent{
                ControlEventKind::transport_stop, 0.0F, timestamp};
        case MidiMessageKind::continue_playback:
            return ControlEvent{
                ControlEventKind::transport_continue, 0.0F, timestamp};
        case MidiMessageKind::control_change:
            break;
        case MidiMessageKind::program_change:
        case MidiMessageKind::unsupported:
            return std::nullopt;
    }

    if (message.channel != 1U || message.data2 > 127U) {
        return std::nullopt;
    }

    ControlEventKind kind{};
    if (message.data1 == 20U) {
        kind = ControlEventKind::primary_normalized;
    } else if (message.data1 == 21U) {
        kind = ControlEventKind::secondary_normalized;
    } else {
        return std::nullopt;
    }

    return ControlEvent{
        kind,
        static_cast<float>(message.data2) / 127.0F,
        timestamp,
    };
}

}  // namespace dsp::targets::daisy
