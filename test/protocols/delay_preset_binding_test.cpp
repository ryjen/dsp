#include <dsp/effects/delay.hpp>
#include <dsp/protocols/delay_preset_binding.hpp>
#include <dsp/protocols/guitar_dsp_preset_v1.hpp>

#include <cmath>
#include <string_view>

namespace {
constexpr std::string_view fixture =
    R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";

bool near(float actual, float expected, float tolerance = 1.0e-4F) {
    return std::abs(actual - expected) <= tolerance;
}
}

int main() {
    using dsp::effects::DelayProcessor;
    using dsp::effects::DelaySubdivision;
    using namespace dsp::protocols;

    const auto parsed = parse_preset_v1(fixture);
    if (!parsed) return 1;

    DelayProcessor first;
    apply_delay_preset_v1(parsed.value, first);
    if (!near(first.tempo_bpm(), 92.0F)) return 2;
    if (!near(first.delay_ms(), 375.0F)) return 3;
    if (!near(first.feedback(), 0.320F)) return 4;
    if (!near(first.mix(), 0.500F)) return 5;
    if (first.subdivision() != DelaySubdivision::dotted_eighth) return 6;

    auto renamed = parsed.value;
    if (set_preset_name(renamed, "different-name") != ProtocolError::none) return 7;
    DelayProcessor second;
    apply_delay_preset_v1(renamed, second);
    if (!near(second.tempo_bpm(), first.tempo_bpm())) return 8;
    if (!near(second.delay_ms(), first.delay_ms())) return 9;
    if (!near(second.feedback(), first.feedback())) return 10;
    if (!near(second.mix(), first.mix())) return 11;
    if (second.subdivision() != first.subdivision()) return 12;

    return 0;
}
