#pragma once

#include <cmath>
#include <span>

namespace dsp::native {

[[nodiscard]] inline bool samples_close(
    std::span<const float> expected,
    std::span<const float> actual,
    float absolute_tolerance,
    float relative_tolerance) noexcept {
    if (expected.size() != actual.size()) return false;
    if (absolute_tolerance < 0.0F || relative_tolerance < 0.0F) return false;

    for (std::size_t index = 0; index < expected.size(); ++index) {
        const float wanted = expected[index];
        const float got = actual[index];
        if (wanted == got) continue;
        if (!std::isfinite(wanted) || !std::isfinite(got)) return false;
        const float difference = std::abs(got - wanted);
        const float allowed = absolute_tolerance + relative_tolerance * std::abs(wanted);
        if (difference > allowed) return false;
    }
    return true;
}

} // namespace dsp::native
