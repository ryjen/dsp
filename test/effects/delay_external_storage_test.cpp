#include <dsp/effects/delay.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <span>
#include <vector>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};
}

void* operator new(std::size_t size) {
    if (tracking.load(std::memory_order_relaxed)) {
        allocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

int main() {
    using dsp::ProcessSpec;
    using dsp::effects::DelayProcessor;

    const ProcessSpec spec{1000.0, 16, 2};
    const auto required = DelayProcessor::required_storage_samples(spec);
    if (!required || *required != 4004U) return 1;

    std::vector<float> too_small(*required - 1U, 0.0F);
    DelayProcessor rejected;
    if (rejected.prepare(spec, std::span<float>{too_small})) return 2;

    std::vector<float> storage(*required, 1.0F);
    DelayProcessor external;
    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    const bool prepared =
        external.prepare(spec, std::span<float>{storage});
    tracking.store(false, std::memory_order_release);

    if (!prepared) return 3;
    if (allocations.load(std::memory_order_relaxed) != 0U) return 4;
    for (float sample : storage) {
        if (sample != 0.0F) return 5;
    }

    external.set_delay_ms(10.0F);
    external.set_feedback(0.0F);
    external.set_mix(1.0F);
    external.reset();

    float samples[16]{};
    samples[0] = 1.0F;
    float* channels[]{samples};
    external.process({channels, 1, 16});
    if (samples[10] != 1.0F) return 6;

    const ProcessSpec invalid{};
    if (DelayProcessor::required_storage_samples(invalid)) return 7;

    return 0;
}
