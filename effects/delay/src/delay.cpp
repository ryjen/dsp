#include <dsp/effects/delay.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace dsp::effects {

class DelayProcessor::Impl {
public:
    bool prepare(
        const ProcessSpec& new_spec,
        float delay_ms,
        float feedback_value,
        float mix_value,
        float bpm,
        DelaySubdivision subdivision) {
        if (!new_spec.valid()) return false;

        const double max_samples = std::ceil(new_spec.sample_rate * 2.0);
        const double size_limit =
            static_cast<double>(std::numeric_limits<std::size_t>::max() - 2);
        if (!std::isfinite(max_samples) || max_samples > size_limit) return false;

        const std::size_t new_capacity =
            static_cast<std::size_t>(max_samples) + 2;
        std::vector<std::vector<float>> new_ring(
            new_spec.channel_count,
            std::vector<float>(new_capacity, 0.0F));

        ring = std::move(new_ring);
        capacity = new_capacity;
        spec = new_spec;
        write_index = 0;
        prepared = true;
        apply_targets(delay_ms, feedback_value, mix_value, bpm, subdivision);
        return true;
    }

    void reset(
        float delay_ms,
        float feedback_value,
        float mix_value,
        float bpm,
        DelaySubdivision subdivision) noexcept {
        if (!prepared) return;
        for (auto& channel : ring) {
            std::fill(channel.begin(), channel.end(), 0.0F);
        }
        write_index = 0;
        apply_targets(delay_ms, feedback_value, mix_value, bpm, subdivision);
    }

    void apply_targets(
        float free_delay_ms,
        float feedback_value,
        float mix_value,
        float bpm,
        DelaySubdivision subdivision) noexcept {
        const float resolved_ms =
            resolve_delay_ms(free_delay_ms, bpm, subdivision);
        const double exact_samples =
            static_cast<double>(resolved_ms) * spec.sample_rate / 1000.0;
        const auto rounded =
            static_cast<std::size_t>(std::llround(exact_samples));
        delay_samples = std::clamp<std::size_t>(rounded, 1, capacity - 2);
        feedback = feedback_value;
        mix = mix_value;
    }

    [[nodiscard]] float read(std::size_t channel) const noexcept {
        const std::size_t read_index =
            (write_index + capacity - delay_samples) % capacity;
        return ring[channel][read_index];
    }

    std::vector<std::vector<float>> ring;
    ProcessSpec spec{};
    std::size_t write_index{};
    std::size_t capacity{};
    std::size_t delay_samples{1};
    float feedback{};
    float mix{};
    bool prepared{};
};

DelayProcessor::DelayProcessor() : impl_(std::make_unique<Impl>()) {}
DelayProcessor::~DelayProcessor() = default;

bool DelayProcessor::prepare(const ProcessSpec& spec) {
    return impl_->prepare(
        spec, delay_ms(), feedback(), mix(), tempo_bpm(), subdivision());
}

void DelayProcessor::reset() noexcept {
    impl_->reset(delay_ms(), feedback(), mix(), tempo_bpm(), subdivision());
}

void DelayProcessor::process(AudioBlock block) noexcept {
    if (!impl_->prepared) return;
    if (block.frame_count > impl_->spec.max_block_size) return;
    if (block.channel_count > impl_->spec.channel_count) return;
    if (block.frame_count == 0 || block.channel_count == 0) return;

    impl_->apply_targets(
        delay_ms(), feedback(), mix(), tempo_bpm(), subdivision());

    for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
        for (std::size_t channel = 0; channel < block.channel_count; ++channel) {
            float* samples = block.channel(channel);
            if (samples == nullptr) continue;
            const float input = samples[frame];
            const float delayed = impl_->read(channel);
            impl_->ring[channel][impl_->write_index] =
                input + delayed * impl_->feedback;
            samples[frame] =
                input * (1.0F - impl_->mix) + delayed * impl_->mix;
        }
        impl_->write_index = (impl_->write_index + 1) % impl_->capacity;
    }
}

void DelayProcessor::set_delay_ms(float value) noexcept {
    if (!std::isfinite(value)) return;
    delay_ms_.store(std::clamp(value, 1.0F, max_delay_ms));
}

void DelayProcessor::set_feedback(float value) noexcept {
    if (!std::isfinite(value)) return;
    feedback_.store(std::clamp(value, 0.0F, max_feedback));
}

void DelayProcessor::set_mix(float value) noexcept {
    if (!std::isfinite(value)) return;
    mix_.store(std::clamp(value, 0.0F, 1.0F));
}

void DelayProcessor::set_tempo_bpm(float value) noexcept {
    if (!std::isfinite(value)) return;
    tempo_bpm_.store(std::clamp(value, 20.0F, 300.0F));
}

void DelayProcessor::set_subdivision(DelaySubdivision value) noexcept {
    subdivision_.store(
        static_cast<std::uint32_t>(value),
        std::memory_order_release);
}

float DelayProcessor::delay_ms() const noexcept { return delay_ms_.load(); }
float DelayProcessor::feedback() const noexcept { return feedback_.load(); }
float DelayProcessor::mix() const noexcept { return mix_.load(); }
float DelayProcessor::tempo_bpm() const noexcept { return tempo_bpm_.load(); }

DelaySubdivision DelayProcessor::subdivision() const noexcept {
    return static_cast<DelaySubdivision>(
        subdivision_.load(std::memory_order_acquire));
}

}  // namespace dsp::effects