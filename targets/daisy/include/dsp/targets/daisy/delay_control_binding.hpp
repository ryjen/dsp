#pragma once

#include <dsp/control/pedal_control.hpp>

namespace dsp::effects {
class DelayProcessor;
}

namespace dsp::targets::daisy {

class DelayControlBinding {
public:
    static void apply(
        dsp::effects::DelayProcessor& delay,
        const dsp::control::PedalControlSnapshot& controls) noexcept;
};

}  // namespace dsp::targets::daisy
