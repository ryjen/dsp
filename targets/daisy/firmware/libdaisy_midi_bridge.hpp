#pragma once

#include "hid/MidiEvent.h"

#include <dsp/targets/daisy/midi_adapter.hpp>

namespace dsp::targets::daisy::firmware {

class LibDaisyMidiBridge {
public:
    [[nodiscard]] static MidiMessage translate(
        const ::daisy::MidiEvent& event) noexcept;
};

}  // namespace dsp::targets::daisy::firmware
