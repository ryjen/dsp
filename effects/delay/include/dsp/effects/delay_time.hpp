#pragma once

#include <algorithm>
#include <cstdint>

namespace dsp::effects {

enum class DelaySubdivision : std::uint32_t {
    free,
    quarter,
    dotted_eighth,
    eighth,
    eighth_triplet,
    sixteenth,
};

[[nodiscard]] inline float resolve_delay_ms(
    float free_ms,
    float bpm,
    DelaySubdivision subdivision) noexcept {
    const float safe_free = std::clamp(free_ms, 1.0F, 2000.0F);
    if (subdivision == DelaySubdivision::free) return safe_free;

    const float safe_bpm = std::clamp(bpm, 20.0F, 300.0F);
    float beats = 0.0F;
    switch (subdivision) {
        case DelaySubdivision::quarter: beats = 1.0F; break;
        case DelaySubdivision::dotted_eighth: beats = 0.75F; break;
        case DelaySubdivision::eighth: beats = 0.5F; break;
        case DelaySubdivision::eighth_triplet: beats = 1.0F / 3.0F; break;
        case DelaySubdivision::sixteenth: beats = 0.25F; break;
        case DelaySubdivision::free: return safe_free;
        default: return safe_free;
    }
    return std::clamp((60000.0F / safe_bpm) * beats, 1.0F, 2000.0F);
}

}  // namespace dsp::effects
