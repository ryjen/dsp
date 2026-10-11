#include <dsp/analysis/performance_analyzer.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace dsp::analysis {
namespace {

std::uint16_t amplitude_to_milli(float amplitude) noexcept {
    const float scaled = std::clamp(
        amplitude * 1000.0F,
        0.0F,
        static_cast<float>(std::numeric_limits<std::uint16_t>::max()));
    return static_cast<std::uint16_t>(std::lround(scaled));
}

}  // namespace

bool PerformanceAnalyzerConfig::valid() const noexcept {
    return std::isfinite(onset_threshold) &&
           std::isfinite(release_threshold) &&
           std::isfinite(envelope_release_ms) &&
           std::isfinite(clipping_threshold) &&
           onset_threshold > 0.0F &&
           onset_threshold <= 1.0F &&
           release_threshold > 0.0F &&
           release_threshold < onset_threshold &&
           envelope_release_ms > 0.0F &&
           minimum_event_ms > 0U &&
           clipping_threshold > onset_threshold &&
           clipping_threshold <= 1.0F;
}

PerformanceAnalyzer::PerformanceAnalyzer(
    PerformanceObservationQueue& output,
    PerformanceAnalyzerConfig config) noexcept
    : output_{output}, config_{config} {}

bool PerformanceAnalyzer::prepare(const dsp::ProcessSpec& spec) {
    if (!spec.valid() || spec.channel_count != 1U || !config_.valid()) {
        prepared_ = false;
        return false;
    }

    sample_rate_ = spec.sample_rate;
    max_block_size_ = spec.max_block_size;
    const double release_samples =
        (static_cast<double>(config_.envelope_release_ms) / 1000.0) *
        sample_rate_;
    envelope_release_coefficient_ = static_cast<float>(
        std::pow(0.01, 1.0 / std::max(1.0, release_samples)));
    minimum_event_frames_ = static_cast<std::uint64_t>(
        std::ceil(
            (static_cast<double>(config_.minimum_event_ms) / 1000.0) *
            sample_rate_));
    prepared_ = true;
    return true;
}

void PerformanceAnalyzer::reset() noexcept {
    frame_index_ = 0U;
    event_start_frame_ = 0U;
    event_peak_frame_ = 0U;
    previous_event_end_frame_ = 0U;
    envelope_ = 0.0F;
    event_peak_ = 0.0F;
    event_quality_flags_ = 0U;
    event_active_ = false;
    have_previous_event_ = false;
    dropped_observations_.store(0U, std::memory_order_release);
}

std::uint64_t PerformanceAnalyzer::frames_to_ms(
    std::uint64_t frames) const noexcept {
    if (sample_rate_ <= 0.0) return 0U;
    const double milliseconds =
        (static_cast<double>(frames) * 1000.0) / sample_rate_;
    if (milliseconds >=
        static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(std::llround(milliseconds));
}

void PerformanceAnalyzer::finish_event(std::uint64_t end_frame) noexcept {
    const std::uint64_t duration_frames = end_frame - event_start_frame_;
    if (duration_frames < minimum_event_frames_) {
        event_quality_flags_ |=
            quality_mask(ObservationQualityFlag::short_event);
    }
    if (event_peak_ >= config_.clipping_threshold) {
        event_quality_flags_ |=
            quality_mask(ObservationQualityFlag::clipped);
    }

    PerformanceObservationV1 observation;
    observation.quality_flags = event_quality_flags_;
    observation.start_frame = event_start_frame_;
    observation.end_frame = end_frame;
    observation.timestamp_ms = frames_to_ms(event_start_frame_);
    observation.attack_ms =
        frames_to_ms(event_peak_frame_ - event_start_frame_);
    observation.duration_ms = frames_to_ms(duration_frames);
    observation.peak_milli = amplitude_to_milli(event_peak_);
    if (have_previous_event_) {
        observation.has_inter_event_spacing = 1U;
        observation.inter_event_spacing_ms =
            frames_to_ms(event_start_frame_ - previous_event_end_frame_);
    }

    if (!output_.push(observation)) {
        dropped_observations_.fetch_add(1U, std::memory_order_relaxed);
    }

    previous_event_end_frame_ = end_frame;
    have_previous_event_ = true;
    event_active_ = false;
    event_peak_ = 0.0F;
    event_quality_flags_ = 0U;
}

void PerformanceAnalyzer::process(dsp::AudioBlock block) noexcept {
    if (!prepared_ ||
        block.channel_count != 1U ||
        block.frame_count > max_block_size_ ||
        block.channels == nullptr ||
        block.channel(0U) == nullptr) {
        return;
    }

    const float* input = block.channel(0U);
    for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
        float magnitude = 0.0F;
        const float sample = input[frame];
        if (std::isfinite(sample)) {
            magnitude = std::fabs(sample);
        } else if (event_active_) {
            event_quality_flags_ |=
                quality_mask(ObservationQualityFlag::non_finite_input);
        }

        envelope_ = std::max(
            magnitude,
            envelope_ * envelope_release_coefficient_);

        if (!event_active_ && envelope_ >= config_.onset_threshold) {
            event_active_ = true;
            event_start_frame_ = frame_index_;
            event_peak_frame_ = frame_index_;
            event_peak_ = magnitude;
            event_quality_flags_ = 0U;
        }

        if (event_active_) {
            if (!std::isfinite(sample)) {
                event_quality_flags_ |=
                    quality_mask(ObservationQualityFlag::non_finite_input);
            }
            if (magnitude > event_peak_) {
                event_peak_ = magnitude;
                event_peak_frame_ = frame_index_;
            }
            if (envelope_ < config_.release_threshold) {
                finish_event(frame_index_);
            }
        }

        ++frame_index_;
    }
}

}  // namespace dsp::analysis
