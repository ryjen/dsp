#pragma once

#include <dsp/analysis/performance_observation_queue.hpp>
#include <dsp/processor.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace dsp::analysis {

struct PerformanceAnalyzerConfig {
    float onset_threshold{0.08F};
    float release_threshold{0.03F};
    float envelope_release_ms{12.0F};
    std::uint32_t minimum_event_ms{25U};
    float clipping_threshold{0.995F};

    [[nodiscard]] bool valid() const noexcept;
};

class PerformanceAnalyzer final : public dsp::Processor {
public:
    explicit PerformanceAnalyzer(
        PerformanceObservationQueue& output,
        PerformanceAnalyzerConfig config = {}) noexcept;

    bool prepare(const dsp::ProcessSpec& spec) override;
    void reset() noexcept override;
    void process(dsp::AudioBlock block) noexcept override;

    [[nodiscard]] std::uint32_t dropped_observations() const noexcept {
        return dropped_observations_.load(std::memory_order_acquire);
    }

private:
    [[nodiscard]] std::uint64_t frames_to_ms(
        std::uint64_t frames) const noexcept;
    void finish_event(std::uint64_t end_frame) noexcept;

    PerformanceObservationQueue& output_;
    PerformanceAnalyzerConfig config_;
    double sample_rate_{};
    std::size_t max_block_size_{};
    float envelope_release_coefficient_{};
    std::uint64_t minimum_event_frames_{};
    std::uint64_t frame_index_{};
    std::uint64_t event_start_frame_{};
    std::uint64_t event_peak_frame_{};
    std::uint64_t previous_event_end_frame_{};
    float envelope_{};
    float event_peak_{};
    std::uint8_t event_quality_flags_{};
    bool prepared_{};
    bool event_active_{};
    bool have_previous_event_{};
    std::atomic<std::uint32_t> dropped_observations_{0U};
};

}  // namespace dsp::analysis
