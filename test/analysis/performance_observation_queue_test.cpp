#include <dsp/analysis/performance_observation_queue.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};
}

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

    PerformanceObservationQueue queue;
    for (std::uint32_t i = 0U;
         i < PerformanceObservationQueue::capacity;
         ++i) {
        PerformanceObservationV1 observation;
        observation.start_frame = i;
        observation.peak_milli =
            static_cast<std::uint16_t>(100U + i);
        if (!queue.push(observation)) return 1;
    }
    if (queue.push({})) return 2;

    for (std::uint32_t i = 0U;
         i < PerformanceObservationQueue::capacity;
         ++i) {
        const auto observation = queue.pop();
        if (!observation) return 3;
        if (observation->start_frame != i) return 4;
        if (observation->peak_milli !=
            static_cast<std::uint16_t>(100U + i)) {
            return 5;
        }
    }
    if (queue.pop()) return 6;

    allocations.store(0U, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::uint32_t i = 0U; i < 10000U; ++i) {
        PerformanceObservationV1 observation;
        observation.start_frame = i;
        if (!queue.push(observation)) return 7;
        const auto popped = queue.pop();
        if (!popped || popped->start_frame != i) return 8;
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 9;

    return 0;
}
