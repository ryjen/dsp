#include <dsp/effects/delay.hpp>
#include <dsp/targets/daisy/pedal_runtime.hpp>

#include <array>
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
    dsp::effects::DelayProcessor delay;
    delay.set_delay_ms(375.0F);
    delay.set_feedback(0.65F);
    delay.set_mix(0.5F);

    dsp::targets::daisy::PedalRuntime runtime;
    if (!runtime.prepare({48000.0, 48, 2}, delay)) return 1;

    std::array<float, 48> left_in{};
    std::array<float, 48> right_in{};
    std::array<float, 48> left_out{};
    std::array<float, 48> right_out{};
    left_in.fill(0.25F);
    right_in.fill(-0.25F);
    const float* inputs[]{left_in.data(), right_in.data()};
    float* outputs[]{left_out.data(), right_out.data()};

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    void* probe = ::operator new(16);
    ::operator delete(probe);
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 1U) return 2;

    allocations.store(0, std::memory_order_relaxed);
    for (std::size_t callback = 0; callback < 2000; ++callback) {
        runtime.set_bypassed(callback % 2U != 0U);
        tracking.store(true, std::memory_order_release);
        runtime.process(inputs, outputs, 2, 48);
        tracking.store(false, std::memory_order_release);
    }
    if (allocations.load(std::memory_order_relaxed) != 0U) return 3;

    return 0;
}
