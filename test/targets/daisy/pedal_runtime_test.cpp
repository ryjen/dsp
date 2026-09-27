#include <dsp/effects/delay.hpp>
#include <dsp/targets/daisy/pedal_runtime.hpp>

#include <array>
#include <cstddef>

namespace {

class GainProcessor final : public dsp::Processor {
public:
    bool prepare(const dsp::ProcessSpec& spec) override {
        spec_ = spec;
        prepared_ = spec.valid();
        return prepared_;
    }

    void reset() noexcept override {}

    void process(dsp::AudioBlock block) noexcept override {
        if (!prepared_) return;
        for (std::size_t channel = 0; channel < block.channel_count; ++channel) {
            float* samples = block.channel(channel);
            if (samples == nullptr) continue;
            for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
                samples[frame] *= 2.0F;
            }
        }
    }

private:
    dsp::ProcessSpec spec_{};
    bool prepared_{false};
};

bool all_equal(const std::array<float, 48>& values, float expected) {
    for (float value : values) {
        if (value != expected) return false;
    }
    return true;
}

}  // namespace

int main() {
    using dsp::targets::daisy::PedalRuntime;

    GainProcessor gain;
    PedalRuntime runtime;
    if (runtime.prepare({0.0, 48, 2}, gain)) return 1;
    if (!runtime.prepare({48000.0, 48, 2}, gain)) return 2;

    std::array<float, 48> left_in{};
    std::array<float, 48> right_in{};
    std::array<float, 48> left_out{};
    std::array<float, 48> right_out{};
    left_in.fill(0.25F);
    right_in.fill(-0.25F);
    const float* inputs[]{left_in.data(), right_in.data()};
    float* outputs[]{left_out.data(), right_out.data()};

    runtime.process(inputs, outputs, 2, 48);
    if (!all_equal(left_out, 0.5F) || !all_equal(right_out, -0.5F)) return 3;

    runtime.set_bypassed(true);
    for (int block = 0; block < 5; ++block) {
        left_out.fill(0.0F);
        right_out.fill(0.0F);
        runtime.process(inputs, outputs, 2, 48);
    }
    left_out.fill(0.0F);
    right_out.fill(0.0F);
    runtime.process(inputs, outputs, 2, 48);
    if (!all_equal(left_out, 0.25F) || !all_equal(right_out, -0.25F)) return 4;

    runtime.set_bypassed(true);
    left_out.fill(0.0F);
    right_out.fill(0.0F);
    runtime.process(inputs, outputs, 2, 48);
    if (!all_equal(left_out, 0.25F) || !all_equal(right_out, -0.25F)) return 5;

    dsp::effects::DelayProcessor delay;
    delay.set_delay_ms(1.0F);
    delay.set_feedback(0.5F);
    delay.set_mix(1.0F);
    PedalRuntime tail_runtime;
    tail_runtime.set_bypassed(true);
    if (!tail_runtime.prepare({48000.0, 48, 1}, delay)) return 6;

    std::array<float, 48> impulse{};
    std::array<float, 48> first_out{};
    impulse[0] = 1.0F;
    const float* impulse_input[]{impulse.data()};
    float* first_output[]{first_out.data()};
    tail_runtime.process(impulse_input, first_output, 1, 48);
    if (first_out[0] != 1.0F) return 7;

    tail_runtime.set_bypassed(false);
    std::array<float, 48> silence{};
    std::array<float, 48> tail_out{};
    const float* silence_input[]{silence.data()};
    float* tail_output[]{tail_out.data()};
    tail_runtime.process(silence_input, tail_output, 1, 48);
    if (!(tail_out[0] > 0.0F)) return 8;

    std::array<float, 49> oversized_in{};
    std::array<float, 49> oversized_out{};
    oversized_out.fill(3.0F);
    const float* oversized_inputs[]{oversized_in.data()};
    float* oversized_outputs[]{oversized_out.data()};
    runtime.process(oversized_inputs, oversized_outputs, 1, 49);
    for (float value : oversized_out) {
        if (value != 3.0F) return 9;
    }

    std::array<float, 48> valid_in{};
    std::array<float, 48> valid_out{};
    valid_in.fill(0.125F);
    const float* null_inputs[]{nullptr, valid_in.data()};
    float* null_outputs[]{nullptr, valid_out.data()};
    runtime.set_bypassed(false);
    runtime.process(null_inputs, null_outputs, 2, 48);
    if (!all_equal(valid_out, 0.25F)) return 10;

    return 0;
}
