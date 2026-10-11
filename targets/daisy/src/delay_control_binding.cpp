#include <dsp/targets/daisy/delay_control_binding.hpp>

#include <dsp/effects/delay.hpp>

#include <algorithm>
#include <cmath>

namespace dsp::targets::daisy {

void DelayControlBinding::apply(
    dsp::effects::DelayProcessor& delay,
    const dsp::control::PedalControlSnapshot& controls) noexcept {
    const float primary = std::clamp(controls.primary_normalized, 0.0F, 1.0F);
    const float secondary =
        std::clamp(controls.secondary_normalized, 0.0F, 1.0F);

    delay.set_delay_ms(std::exp(std::log(2000.0F) * primary));
    delay.set_feedback(dsp::effects::DelayProcessor::max_feedback * secondary);
    delay.set_tempo_bpm(controls.tempo_bpm);

    using dsp::control::PedalMode;
    using dsp::effects::DelaySubdivision;
    switch (controls.mode) {
        case PedalMode::quarter:
            delay.set_subdivision(DelaySubdivision::quarter);
            break;
        case PedalMode::dotted_eighth:
            delay.set_subdivision(DelaySubdivision::dotted_eighth);
            break;
        case PedalMode::free:
        default:
            delay.set_subdivision(DelaySubdivision::free);
            break;
    }
}

}  // namespace dsp::targets::daisy
