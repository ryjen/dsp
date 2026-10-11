#include <dsp/targets/daisy/pedal_runtime.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace dsp::targets::daisy {

bool PedalRuntime::prepare(
    const dsp::ProcessSpec& spec,
    dsp::Processor& processor) {
    prepared_ = false;
    processor_ = nullptr;

    if (!spec.valid()) return false;
    if (spec.channel_count >
        std::numeric_limits<std::size_t>::max() / spec.max_block_size) {
        return false;
    }

    std::vector<float> new_dry_scratch(
        spec.channel_count * spec.max_block_size, 0.0F);
    std::vector<float*> new_process_channels(spec.channel_count, nullptr);

    if (!processor.prepare(spec)) return false;
    processor.reset();

    spec_ = spec;
    processor_ = &processor;
    dry_scratch_ = std::move(new_dry_scratch);
    process_channels_ = std::move(new_process_channels);

    last_bypass_target_ =
        bypassed_.load(std::memory_order_acquire) != 0U ? 1.0F : 0.0F;
    bypass_mix_.reset(last_bypass_target_);
    prepared_ = true;
    return true;
}

void PedalRuntime::set_bypassed(bool bypassed) noexcept {
    bypassed_.store(bypassed ? 1U : 0U, std::memory_order_release);
}

void PedalRuntime::process(
    const float* const* input,
    float* const* output,
    std::size_t channels,
    std::size_t frames) noexcept {
    if (!prepared_ || processor_ == nullptr) return;
    if (input == nullptr || output == nullptr) return;
    if (channels == 0U || frames == 0U) return;
    if (channels > spec_.channel_count || frames > spec_.max_block_size) return;

    const float bypass_target =
        bypassed_.load(std::memory_order_acquire) != 0U ? 1.0F : 0.0F;
    if (bypass_target != last_bypass_target_) {
        last_bypass_target_ = bypass_target;
        bypass_mix_.set_target(bypass_target, bypass_ramp_samples());
    }

    for (std::size_t channel = 0; channel < channels; ++channel) {
        process_channels_[channel] = nullptr;
        if (input[channel] == nullptr || output[channel] == nullptr) {
            continue;
        }

        const std::size_t base = channel * spec_.max_block_size;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const float sample = input[channel][frame];
            dry_scratch_[base + frame] = sample;
            output[channel][frame] = sample;
        }
        process_channels_[channel] = output[channel];
    }

    processor_->process({process_channels_.data(), channels, frames});

    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float dry_mix = bypass_mix_.next();
        const float wet_mix = 1.0F - dry_mix;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            if (process_channels_[channel] == nullptr) continue;
            const std::size_t index =
                channel * spec_.max_block_size + frame;
            output[channel][frame] =
                output[channel][frame] * wet_mix +
                dry_scratch_[index] * dry_mix;
        }
    }
}

std::size_t PedalRuntime::bypass_ramp_samples() const noexcept {
    const double samples = std::round(spec_.sample_rate * 0.005);
    if (samples <= 1.0) return 1U;
    const double maximum =
        static_cast<double>(std::numeric_limits<std::size_t>::max());
    if (samples >= maximum) {
        return std::numeric_limits<std::size_t>::max();
    }
    return static_cast<std::size_t>(samples);
}

}  // namespace dsp::targets::daisy
