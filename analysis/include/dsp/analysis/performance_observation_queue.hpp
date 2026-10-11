#pragma once

#include <dsp/analysis/performance_observation.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

namespace dsp::analysis {

class PerformanceObservationQueue {
public:
    static constexpr std::uint32_t capacity = 64U;

    bool push(const PerformanceObservationV1& observation) noexcept {
        const std::uint32_t write =
            write_sequence_.load(std::memory_order_relaxed);
        const std::uint32_t read =
            read_sequence_.load(std::memory_order_acquire);
        if (static_cast<std::uint32_t>(write - read) >= capacity) {
            return false;
        }

        entries_[write % capacity] = observation;
        write_sequence_.store(write + 1U, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<PerformanceObservationV1> pop() noexcept {
        const std::uint32_t read =
            read_sequence_.load(std::memory_order_relaxed);
        const std::uint32_t write =
            write_sequence_.load(std::memory_order_acquire);
        if (read == write) return std::nullopt;

        const PerformanceObservationV1 item = entries_[read % capacity];
        read_sequence_.store(read + 1U, std::memory_order_release);
        return item;
    }

private:
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    std::array<PerformanceObservationV1, capacity> entries_{};
    std::atomic<std::uint32_t> write_sequence_{0U};
    std::atomic<std::uint32_t> read_sequence_{0U};
};

}  // namespace dsp::analysis
