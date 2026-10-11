#include <dsp/protocols/delay_preset_binding.hpp>

#include <dsp/effects/delay.hpp>

namespace dsp::protocols {
namespace {

dsp::effects::DelaySubdivision to_delay_subdivision(
    PresetSubdivisionV1 value) noexcept {
    using Target = dsp::effects::DelaySubdivision;
    switch (value) {
        case PresetSubdivisionV1::free:
            return Target::free;
        case PresetSubdivisionV1::quarter:
            return Target::quarter;
        case PresetSubdivisionV1::dotted_eighth:
            return Target::dotted_eighth;
        case PresetSubdivisionV1::eighth:
            return Target::eighth;
        case PresetSubdivisionV1::eighth_triplet:
            return Target::eighth_triplet;
        case PresetSubdivisionV1::sixteenth:
            return Target::sixteenth;
    }
    return Target::free;
}

}  // namespace

ProtocolError apply_delay_preset_v1(
    const DelayPresetV1& preset,
    dsp::effects::DelayProcessor& processor) noexcept {
    const auto error = validate_preset_v1(preset);
    if (error != ProtocolError::none) return error;

    processor.set_delay_ms(static_cast<float>(preset.free_ms));
    processor.set_feedback(static_cast<float>(preset.feedback_milli) / 1000.0F);
    processor.set_mix(static_cast<float>(preset.mix_milli) / 1000.0F);
    processor.set_tempo_bpm(static_cast<float>(preset.tempo_bpm));
    processor.set_subdivision(to_delay_subdivision(preset.subdivision));
    return ProtocolError::none;
}

}  // namespace dsp::protocols
