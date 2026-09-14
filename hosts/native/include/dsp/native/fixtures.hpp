#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace dsp::native {

[[nodiscard]] inline std::vector<float> make_silence(std::size_t frames) {
    return std::vector<float>(frames, 0.0F);
}

[[nodiscard]] inline std::vector<float> make_constant(std::size_t frames, float value) {
    return std::vector<float>(frames, value);
}

[[nodiscard]] inline std::vector<float> make_impulse(
    std::size_t frames,
    std::size_t index = 0,
    float amplitude = 1.0F) {
    std::vector<float> result(frames, 0.0F);
    if (index < frames) result[index] = amplitude;
    return result;
}

[[nodiscard]] inline std::vector<float> make_sine(
    std::size_t frames,
    double frequency,
    double sample_rate,
    double phase = 0.0) {
    if (sample_rate <= 0.0) return {};
    std::vector<float> result(frames);
    const double step = 2.0 * std::numbers::pi_v<double> * frequency / sample_rate;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        result[frame] = static_cast<float>(std::sin(phase + step * static_cast<double>(frame)));
    }
    return result;
}

} // namespace dsp::native
