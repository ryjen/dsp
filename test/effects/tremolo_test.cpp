#include <dsp/effects/tremolo.hpp>
#include <dsp/native/fixtures.hpp>
#include <dsp/native/offline_renderer.hpp>
#include <dsp/native/sample_compare.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

using dsp::effects::TremoloProcessor;

bool all_close(const std::vector<float>& a, const std::vector<float>& b) {
    return dsp::native::samples_close(a, b, 1.0e-6F, 1.0e-5F);
}

}  // namespace

int main() {
    TremoloProcessor tremolo;
    tremolo.set_rate_hz(-4.0F);
    tremolo.set_depth(2.0F);
    tremolo.set_shape(-1.0F);
    if (tremolo.rate_hz() != 0.1F) return 1;
    if (tremolo.depth() != 1.0F) return 2;
    if (tremolo.shape() != 0.0F) return 3;
    tremolo.set_rate_hz(25.0F);
    if (tremolo.rate_hz() != 20.0F) return 4;

    // Depth zero is exact passthrough, including nontrivial sine input.
    tremolo.set_depth(0.0F);
    const auto sine = dsp::native::make_sine(4096, 440.0, 48000.0);
    const std::vector<std::vector<float>> sine_input{sine, sine};
    auto result = dsp::native::render_offline(tremolo, {48000.0, 64, 2}, sine_input, 64);
    if (!result || result.channels != sine_input) return 5;

    // Silence must remain silence at maximum modulation depth.
    tremolo.set_depth(1.0F);
    const std::vector<std::vector<float>> silence{dsp::native::make_silence(4096)};
    result = dsp::native::render_offline(tremolo, {48000.0, 64, 1}, silence, 64);
    if (!result) return 6;
    if (std::any_of(result.channels[0].begin(), result.channels[0].end(), [](float v) { return v != 0.0F; })) return 7;

    // Full-depth DC input stays bounded and demonstrates meaningful modulation.
    tremolo.set_rate_hz(4.0F);
    tremolo.set_shape(0.0F);
    const std::vector<std::vector<float>> dc{dsp::native::make_constant(48000, 1.0F)};
    result = dsp::native::render_offline(tremolo, {48000.0, 128, 1}, dc, 128);
    if (!result) return 8;
    const auto [minimum, maximum] = std::minmax_element(result.channels[0].begin(), result.channels[0].end());
    if (*minimum < -1.0e-5F || *maximum > 1.00001F) return 9;
    if (*maximum - *minimum < 0.25F) return 10;

    // Resetting identical processors with identical controls is deterministic.
    TremoloProcessor first;
    TremoloProcessor second;
    first.set_rate_hz(6.0F); first.set_depth(0.8F); first.set_shape(0.35F);
    second.set_rate_hz(6.0F); second.set_depth(0.8F); second.set_shape(0.35F);
    const std::vector<std::vector<float>> deterministic_input{dsp::native::make_sine(8192, 220.0, 48000.0)};
    const auto first_result = dsp::native::render_offline(first, {48000.0, 128, 1}, deterministic_input, 64);
    const auto second_result = dsp::native::render_offline(second, {48000.0, 128, 1}, deterministic_input, 64);
    if (!first_result || !second_result || !all_close(first_result.channels[0], second_result.channels[0])) return 11;

    // Sine and triangle endpoints must be observably different after smoothing settles.
    TremoloProcessor sine_shape;
    TremoloProcessor triangle_shape;
    sine_shape.set_rate_hz(5.0F); sine_shape.set_depth(1.0F); sine_shape.set_shape(0.0F);
    triangle_shape.set_rate_hz(5.0F); triangle_shape.set_depth(1.0F); triangle_shape.set_shape(1.0F);
    const std::vector<std::vector<float>> shape_input{dsp::native::make_constant(8192, 1.0F)};
    const auto sine_shape_result = dsp::native::render_offline(sine_shape, {48000.0, 128, 1}, shape_input, 128);
    const auto triangle_shape_result = dsp::native::render_offline(triangle_shape, {48000.0, 128, 1}, shape_input, 128);
    if (!sine_shape_result || !triangle_shape_result) return 12;
    bool shape_differs = false;
    for (std::size_t i = 4096; i < 8192; ++i) {
        if (std::abs(sine_shape_result.channels[0][i] - triangle_shape_result.channels[0][i]) > 1.0e-3F) {
            shape_differs = true;
            break;
        }
    }
    if (!shape_differs) return 13;

    // A live depth update is smoothed: no click-sized adjacent-sample discontinuity.
    TremoloProcessor transition;
    transition.set_rate_hz(4.0F); transition.set_depth(0.0F); transition.set_shape(0.0F);
    const dsp::ProcessSpec transition_spec{48000.0, 2048, 1};
    if (!transition.prepare(transition_spec)) return 14;
    transition.reset();
    std::vector<float> lead_in(256, 1.0F);
    float* lead_channels[] = {lead_in.data()};
    transition.process({lead_channels, 1, lead_in.size()});
    transition.set_depth(1.0F);
    std::vector<float> changed(2048, 1.0F);
    float* changed_channels[] = {changed.data()};
    transition.process({changed_channels, 1, changed.size()});
    float previous = lead_in.back();
    for (float sample : changed) {
        if (std::abs(sample - previous) > 0.1F) return 15;
        previous = sample;
    }

    return 0;
}
