#include <dsp/effects/delay.hpp>
#include <dsp/protocols/delay_preset_binding.hpp>

#include <cmath>
#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view fixture =
    R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";

int fail(int code) {
    std::cerr << "failure-code=" << code << "\n";
    return code;
}

bool near(float actual, float expected, float tolerance = 1.0e-4F) {
    return std::fabs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    using dsp::effects::DelayProcessor;
    using dsp::effects::DelaySubdivision;
    using namespace dsp::protocols;

    const auto parsed = parse_preset_v1(fixture);
    if (!parsed) return fail(1);

    DelayProcessor delay;
    if (apply_delay_preset_v1(parsed.value, delay) != ProtocolError::none) {
        return fail(2);
    }
    if (!near(delay.tempo_bpm(), 92.0F)) return fail(3);
    if (!near(delay.delay_ms(), 375.0F)) return fail(4);
    if (!near(delay.feedback(), 0.320F)) return fail(5);
    if (!near(delay.mix(), 0.500F)) return fail(6);
    if (delay.subdivision() != DelaySubdivision::dotted_eighth) return fail(7);

    auto relabeled = parsed.value;
    if (set_preset_name(relabeled, "same-controls") != ProtocolError::none) {
        return fail(8);
    }
    if (apply_delay_preset_v1(relabeled, delay) != ProtocolError::none) {
        return fail(9);
    }
    if (!near(delay.tempo_bpm(), 92.0F) ||
        !near(delay.delay_ms(), 375.0F) ||
        !near(delay.feedback(), 0.320F) ||
        !near(delay.mix(), 0.500F) ||
        delay.subdivision() != DelaySubdivision::dotted_eighth) {
        return fail(10);
    }

    delay.set_delay_ms(111.0F);
    delay.set_feedback(0.25F);
    delay.set_mix(0.75F);
    delay.set_tempo_bpm(123.0F);
    delay.set_subdivision(DelaySubdivision::quarter);

    auto invalid = parsed.value;
    invalid.feedback_milli = 951;
    if (apply_delay_preset_v1(invalid, delay) !=
        ProtocolError::invalid_feedback) {
        return fail(11);
    }
    if (!near(delay.delay_ms(), 111.0F)) return fail(12);
    if (!near(delay.feedback(), 0.25F)) return fail(13);
    if (!near(delay.mix(), 0.75F)) return fail(14);
    if (!near(delay.tempo_bpm(), 123.0F)) return fail(15);
    if (delay.subdivision() != DelaySubdivision::quarter) return fail(16);

    return 0;
}
