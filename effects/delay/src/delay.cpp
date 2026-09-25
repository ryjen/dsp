#include <dsp/effects/delay.hpp>
#include <dsp/linear_smoother.hpp>

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
        float free_delay_ms,
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
        reset_targets(
            free_delay_ms, feedback_value, mix_value, bpm, subdivision);
        return true;
    }

    void reset(
        float free_delay_ms,
        float feedback_value,
        float mix_value,
        float bpm,
        DelaySubdivision subdivision) noexcept {
        if (!prepared) return;
        for (auto& channel : ring) {
            std::fill(channel.begin(), channel.end(), 0.0F);
        }
        write_index = 0;
        reset_targets(
            free_delay_ms, feedback_value, mix_value, bpm, subdivision);
    }

    [[nodiscard]] float resolve_delay_samples(
        float free_delay_ms,
        float bpm,
        DelaySubdivision subdivision) const noexcept {
        const float resolved_ms =
            resolve_delay_ms(free_delay_ms, bpm, subdivision);
        const double exact_samples =
            static_cast<double>(resolved_ms) * spec.sample_rate / 1000.0;
        return std::clamp(
            static_cast<float>(exact_samples),
            1.0F,
            static_cast<float>(capacity - 2));
    }

    void reset_targets(
        float free_delay_ms,
        float feedback_value,
        float mix_value,
        float bpm,
        DelaySubdivision subdivision) noexcept {
        last_delay_target =
            resolve_delay_samples(free_delay_ms, bpm, subdivision);
        last_feedback_target = feedback_value;
        last_mix_target = mix_value;
        delay_samples.reset(last_delay_target);
        feedback.reset(last_feedback_target);
        mix.reset(last_mix_target);
    }

    [[nodiscard]] std::size_t ramp_samples(double seconds) const noexcept {
        const double rounded = std::round(spec.sample_rate * seconds);
        if (rounded <= 1.0) return 1;
        const double maximum =
            static_cast<double>(std::numeric_limits<std::size_t>::max());
        if (rounded >= maximum) return std::numeric_limits<std::size_t>::max();
        return static_cast<std::size_t>(rounded);
    }

    void update_targets(
        float free_delay_ms,
        float feedback_value,
        float mix_value,
        float bpm,
        DelaySubdivision subdivision) noexcept {
        const float delay_target =
            resolve_delay_samples(free_delay_ms, bpm, subdivision);
        if (delay_target != last_delay_target) {
            last_delay_target = delay_target;
            delay_samples.set_target(delay_target, ramp_samples(0.100));
        }
        if (feedback_value != last_feedback_target) {
            last_feedback_target = feedback_value;
            feedback.set_target(feedback_value, ramp_samples(0.010));
        }
        if (mix_value != last_mix_target) {
            last_mix_target = mix_value;
            mix.set_target(mix_value, ramp_samples(0.010));
        }
    }

    [[nodiscard]] float read(
        std::size_t channel,
        float delay_value) const noexcept {
        float position =
            static_cast<float>(write_index) - delay_value;
        while (position < 0.0F) {
            position += static_cast<float>(capacity);
        }
        const float floor_position = std::floor(position);
        const auto first =
            static_cast<std::size_t>(floor_position) % capacity;
        const auto second = (first + 1) % capacity;
        const float fraction = position - floor_position;
        return ring[channel][first] * (1.0F - fraction) +
               ring[channel][second] * fraction;
    }

    std::vector<std::vector<float>> ring;
    ProcessSpec spec{};
    std::size_t write_index{};
    std::size_t capacity{};
    LinearSmoother delay_samples;
    LinearSmoother feedback;
    LinearSmoother mix;
    float last_delay_target{};
    float last_feedback_target{};
    float last_mix_target{};
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

    const float published_delay = delay_ms();
    const float published_feedback = feedback();
    const float published_mix = mix();
    const float published_bpm = tempo_bpm();
    const DelaySubdivision published_subdivision = subdivision();

    impl_->update_targets(
        published_delay,
        published_feedback,
        published_mix,
        published_bpm,
        published_subdivision);

    for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
        const float delay_value = impl_->delay_samples.next();
        const float feedback_value = impl_->feedback.next();
        const float mix_value = impl_->mix.next();

        for (std::size_t channel = 0; channel < block.channel_count; ++channel) {
            float* samples = block.channel(channel);
            if (samples == nullptr) continue;

            const float input = samples[frame];
            const float delayed = impl_->read(channel, delay_value);
            impl_->ring[channel][impl_->write_index] =
                input + delayed * feedback_value;
            samples[frame] =
                input * (1.0F - mix_value) + delayed * mix_value;
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
