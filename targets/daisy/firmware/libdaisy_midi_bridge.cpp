#include "libdaisy_midi_bridge.hpp"

namespace dsp::targets::daisy::firmware {

MidiMessage LibDaisyMidiBridge::translate(
    const ::daisy::MidiEvent& event) noexcept {
    MidiMessage message{};

    if (event.type == ::daisy::ControlChange) {
        if (event.channel < 0 || event.channel > 15 ||
            event.data[0] > 127U || event.data[1] > 127U) {
            return message;
        }
        message.kind = MidiMessageKind::control_change;
        message.channel = static_cast<std::uint8_t>(event.channel + 1);
        message.data1 = event.data[0];
        message.data2 = event.data[1];
        message.valid = true;
        return message;
    }

    if (event.type == ::daisy::ProgramChange) {
        if (event.channel < 0 || event.channel > 15) return message;
        message.kind = MidiMessageKind::program_change;
        message.channel = static_cast<std::uint8_t>(event.channel + 1);
        message.data1 = event.data[0];
        message.valid = true;
        return message;
    }

    if (event.type != ::daisy::SystemRealTime) {
        message.kind = MidiMessageKind::unsupported;
        message.valid = true;
        return message;
    }

    switch (event.srt_type) {
        case ::daisy::TimingClock:
            message.kind = MidiMessageKind::clock;
            break;
        case ::daisy::Start:
            message.kind = MidiMessageKind::start;
            break;
        case ::daisy::Continue:
            message.kind = MidiMessageKind::continue_playback;
            break;
        case ::daisy::Stop:
            message.kind = MidiMessageKind::stop;
            break;
        default:
            message.kind = MidiMessageKind::unsupported;
            break;
    }
    message.valid = true;
    return message;
}

}  // namespace dsp::targets::daisy::firmware
