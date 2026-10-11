#include <dsp/analysis/performance_analyzer.hpp>
#include <dsp/native/callback_budget.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace {

std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};

dsp::analysis::PerformanceAnalyzerConfig fixture_config() {
    dsp::analysis::PerformanceAnalyzerConfig config;
    config.onset_threshold = 0.08F;
    config.release_threshold = 0.03F;
    config.envelope_release_ms = 4.0F;
    config.minimum_event_ms = 5U;
    config.clipping_threshold = 0.995F;
    return config;
}

}  // namespace

void* operator new(std::size_t size) {
    if (tracking.load(std::memory_order_relaxed)) {
        allocations.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept {
    ::operator delete(memory);
}

int main() {
    using namespace dsp::analysis;

    if (performance_observation_version != 1U) return 1;

    PerformanceObservationQueue queue;
    PerformanceAnalyzer analyzer{queue, fixture_config()};
    if (!analyzer.prepare({1000.0, 64U, 1U})) return 2;
    analyzer.reset();

    std::array<float, 40> input{};
    input[5] = 0.10F;
    input[6] = 0.20F;
    input[7] = 0.40F;
    input[8] = 0.80F;
    input[9] = 0.80F;
    input[10] = 0.80F;
    input[11] = 0.80F;
    input[12] = 0.80F;

    input[25] = 0.20F;
    input[26] = 0.50F;
    input[27] = 0.60F;
    input[28] = 0.60F;
    input[29] = 0.60F;

    const auto original = input;
    float* channels[] = {input.data()};
    analyzer.process({channels, 1U, input.size()});
    if (input != original) return 3;

    const auto first = queue.pop();
    if (!first) return 4;
    if (first->version != 1U || !first->reliable()) return 5;
    if (first->start_frame != 5U || first->end_frame != 15U) return 6;
    if (first->timestamp_ms != 5U) return 7;
    if (first->attack_ms != 3U) return 8;
    if (first->duration_ms != 10U) return 9;
    if (first->peak_milli != 800U) return 10;
    if (first->has_inter_event_spacing != 0U) return 11;

    const auto second = queue.pop();
    if (!second) return 12;
    if (second->start_frame != 25U || second->end_frame != 32U) return 13;
    if (second->timestamp_ms != 25U) return 14;
    if (second->attack_ms != 2U) return 15;
    if (second->duration_ms != 7U) return 16;
    if (second->peak_milli != 600U) return 17;
    if (second->has_inter_event_spacing != 1U) return 18;
    if (second->inter_event_spacing_ms != 10U) return 19;
    if (queue.pop()) return 20;

    PerformanceObservationQueue uncertain_queue;
    auto uncertain_config = fixture_config();
    uncertain_config.envelope_release_ms = 1.0F;
    uncertain_config.minimum_event_ms = 5U;
    PerformanceAnalyzer uncertain{uncertain_queue, uncertain_config};
    if (!uncertain.prepare({1000.0, 16U, 1U})) return 21;
    uncertain.reset();

    std::array<float, 4> clipped{1.0F, 0.0F, 0.0F, 0.0F};
    float* clipped_channels[] = {clipped.data()};
    uncertain.process({clipped_channels, 1U, clipped.size()});
    const auto clipped_event = uncertain_queue.pop();
    if (!clipped_event) return 22;
    if (!clipped_event->has_quality_flag(
            ObservationQualityFlag::clipped)) return 23;
    if (!clipped_event->has_quality_flag(
            ObservationQualityFlag::short_event)) return 24;
    if (clipped_event->reliable()) return 25;

    PerformanceObservationQueue nonfinite_queue;
    PerformanceAnalyzer nonfinite{nonfinite_queue, uncertain_config};
    if (!nonfinite.prepare({1000.0, 16U, 1U})) return 26;
    nonfinite.reset();
    std::array<float, 4> nonfinite_input{
        0.5F,
        std::numeric_limits<float>::quiet_NaN(),
        0.0F,
        0.0F,
    };
    float* nonfinite_channels[] = {nonfinite_input.data()};
    nonfinite.process(
        {nonfinite_channels, 1U, nonfinite_input.size()});
    const auto nonfinite_event = nonfinite_queue.pop();
    if (!nonfinite_event) return 27;
    if (!nonfinite_event->has_quality_flag(
            ObservationQualityFlag::non_finite_input)) return 28;

    PerformanceObservationQueue full_queue;
    PerformanceObservationV1 placeholder;
    for (std::uint32_t i = 0U;
         i < PerformanceObservationQueue::capacity;
         ++i) {
        if (!full_queue.push(placeholder)) return 29;
    }
    if (full_queue.push(placeholder)) return 30;

    PerformanceAnalyzer dropping{full_queue, uncertain_config};
    if (!dropping.prepare({1000.0, 16U, 1U})) return 31;
    dropping.reset();
    std::array<float, 4> event{0.5F, 0.0F, 0.0F, 0.0F};
    float* event_channels[] = {event.data()};
    dropping.process({event_channels, 1U, event.size()});
    if (dropping.dropped_observations() != 1U) return 32;

    PerformanceObservationQueue allocation_queue;
    PerformanceAnalyzer allocation_analyzer{
        allocation_queue,
        uncertain_config,
    };
    if (!allocation_analyzer.prepare({1000.0, 16U, 1U})) return 33;
    allocation_analyzer.reset();
    std::array<float, 16> allocation_input{};
    allocation_input[0] = 0.5F;
    float* allocation_channels[] = {allocation_input.data()};
    allocations.store(0U, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000U; ++i) {
        allocation_analyzer.process(
            {allocation_channels, 1U, allocation_input.size()});
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 34;

    PerformanceObservationQueue budget_queue;
    PerformanceAnalyzer budget_analyzer{budget_queue};
    const auto budget = dsp::native::measure_callback_budget(
        budget_analyzer,
        {48000.0, 64U, 1U},
        64U,
        10000U);
    if (!budget) return 35;
    if (!std::isfinite(budget.report.average_deadline_fraction) ||
        budget.report.average_deadline_fraction < 0.0) {
        return 36;
    }

    PerformanceObservationQueue invalid_queue;
    PerformanceAnalyzer invalid{invalid_queue};
    if (invalid.prepare({48000.0, 64U, 2U})) return 37;

    auto invalid_config = fixture_config();
    invalid_config.release_threshold = invalid_config.onset_threshold;
    PerformanceAnalyzer invalid_thresholds{invalid_queue, invalid_config};
    if (invalid_thresholds.prepare({48000.0, 64U, 1U})) return 38;

    return 0;
}
