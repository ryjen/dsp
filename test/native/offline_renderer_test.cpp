#include <dsp/core.hpp>
#include <dsp/native/fixtures.hpp>
#include <dsp/native/offline_renderer.hpp>
#include <dsp/native/sample_compare.hpp>

#include <array>
#include <cmath>
#include <vector>

class RecordingPassthrough final : public dsp::Processor {
public:
    bool prepare(const dsp::ProcessSpec& spec) override {
        prepared = spec.valid() && allow_prepare;
        return prepared;
    }
    void reset() noexcept override { reset_called = true; }
    void process(dsp::AudioBlock block) noexcept override {
        if (callback_count < callback_sizes.size()) {
            callback_sizes[callback_count] = block.frame_count;
        }
        ++callback_count;
    }

    bool allow_prepare{true};
    bool prepared{false};
    bool reset_called{false};
    std::array<std::size_t, 8> callback_sizes{};
    std::size_t callback_count{};
};

int main() {
    const dsp::ProcessSpec spec{48000.0, 4, 2};
    const std::vector<std::vector<float>> input{
        dsp::native::make_constant(10, 0.25F),
        dsp::native::make_constant(10, -0.5F),
    };

    RecordingPassthrough processor;
    const auto rendered = dsp::native::render_offline(processor, spec, input, 4);
    if (!rendered) return 1;
    if (!processor.prepared || !processor.reset_called) return 2;
    if (processor.callback_count != 3) return 3;
    if (processor.callback_sizes[0] != 4 || processor.callback_sizes[1] != 4 || processor.callback_sizes[2] != 2) return 4;
    if (!dsp::native::samples_close(input[0], rendered.channels[0], 0.0F, 0.0F)) return 5;
    if (!dsp::native::samples_close(input[1], rendered.channels[1], 0.0F, 0.0F)) return 6;

    if (dsp::native::render_offline(processor, spec, input, 0).error != dsp::native::RenderError::invalid_block_size) return 7;
    if (dsp::native::render_offline(processor, spec, input, 5).error != dsp::native::RenderError::invalid_block_size) return 8;

    const std::vector<std::vector<float>> wrong_channels{input[0]};
    if (dsp::native::render_offline(processor, spec, wrong_channels, 4).error != dsp::native::RenderError::invalid_input) return 9;

    const std::vector<std::vector<float>> uneven{input[0], std::vector<float>(9, 0.0F)};
    if (dsp::native::render_offline(processor, spec, uneven, 4).error != dsp::native::RenderError::invalid_input) return 10;

    RecordingPassthrough rejected;
    rejected.allow_prepare = false;
    if (dsp::native::render_offline(rejected, spec, input, 4).error != dsp::native::RenderError::prepare_failed) return 11;

    const std::vector<float> expected{1.0F, 2.0F};
    const std::vector<float> close{1.000001F, 1.999999F};
    if (!dsp::native::samples_close(expected, close, 1.0e-5F, 1.0e-5F)) return 12;
    if (dsp::native::samples_close(expected, std::vector<float>{1.1F, 2.0F}, 1.0e-5F, 1.0e-5F)) return 13;

    const auto impulse = dsp::native::make_impulse(5, 2, 0.75F);
    if (impulse.size() != 5 || impulse[2] != 0.75F || impulse[0] != 0.0F) return 14;

    const auto sine = dsp::native::make_sine(5, 12000.0, 48000.0);
    if (sine.size() != 5) return 15;
    if (std::abs(sine[0]) > 1.0e-6F || std::abs(sine[1] - 1.0F) > 1.0e-5F) return 16;
    return 0;
}
