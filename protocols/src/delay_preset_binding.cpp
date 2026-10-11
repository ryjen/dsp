#include <dsp/protocols/delay_preset_binding.hpp>

#include <dsp/effects/delay.hpp>

namespace dsp::protocols {
namespace {

dsp::effects::DelaySubdivision map_subdivision(
    PresetSubdivisionV1 value) noexcept {
    using dsp::effects::DelaySubdivision;

    switch (value) {
        case PresetSubdivisionV1::free:
            return DelaySubdivision::free;
        case PresetSubdivisionV1::quarter:
            return DelaySubdivision::quarter;
        case PresetSubdivisionV1::dotted_eighth:
            return DelaySubdivision::dotted_eighth;
        case PresetSubdivisionV1::eighth:
            return DelaySubdivision::eighth;
        case PresetSubdivisionV1::eighth_triplet:
            return DelaySubdivision::eighth_triplet;
        case PresetSubdivisionV1::sixteenth:
            return DelaySubdivision::sixteenth;
    }
    return DelaySubdivision::free;
}

}  // namespace

void apply_delay_preset_v1(
    const DelayPresetV1& preset,
    dsp::effects::DelayProcessor& delay) noexcept {
    delay.set_tempo_bpm(static_cast<float>(preset.tempo_bpm));
    delay.set_feedback(static_cast<float>(preset.feedback_milli) / 1000.0F);
    delay.set_delay_ms(static_cast<float>(preset.free_ms));
    delay.set_mix(static_cast<float>(preset.mix_milli) / 1000.0F);
    delay.set_subdivision(map_subdivision(preset.subdivision));
}

}  // namespace dsp::protocols
