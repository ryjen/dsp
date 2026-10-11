#include <dsp/control/pedal_control_state.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
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
    using dsp::control::PedalControlSnapshot;
    using dsp::control::PedalControlState;
    using dsp::control::PedalMode;

    PedalControlState state;
    auto snapshot = state.snapshot();
    if (snapshot.primary_normalized != 0.0F) return 1;
    if (snapshot.secondary_normalized != 0.0F) return 2;
    if (snapshot.tempo_bpm != 120.0F) return 3;
    if (snapshot.mode != PedalMode::free) return 4;
    if (snapshot.bypassed || snapshot.transport_running) return 5;

    state.publish({-1.0F, 2.0F, 1000.0F, PedalMode::quarter, true, true});
    snapshot = state.snapshot();
    if (snapshot.primary_normalized != 0.0F) return 6;
    if (snapshot.secondary_normalized != 1.0F) return 7;
    if (snapshot.tempo_bpm != 300.0F) return 8;
    if (snapshot.mode != PedalMode::quarter) return 9;
    if (!snapshot.bypassed || !snapshot.transport_running) return 10;

    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    state.publish({nan, inf, nan, static_cast<PedalMode>(255), false, false});
    snapshot = state.snapshot();
    if (snapshot.primary_normalized != 0.0F) return 11;
    if (snapshot.secondary_normalized != 1.0F) return 12;
    if (snapshot.tempo_bpm != 300.0F) return 13;
    if (snapshot.mode != PedalMode::free) return 14;
    if (snapshot.bypassed || snapshot.transport_running) return 15;

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000; ++i) {
        const float unit = static_cast<float>(i % 101U) / 100.0F;
        state.publish(PedalControlSnapshot{
            unit,
            1.0F - unit,
            20.0F + unit * 280.0F,
            i % 2U == 0U ? PedalMode::free : PedalMode::dotted_eighth,
            i % 2U != 0U,
            i % 3U == 0U,
        });
        const auto current = state.snapshot();
        if (current.primary_normalized < 0.0F || current.primary_normalized > 1.0F) return 16;
        if (current.secondary_normalized < 0.0F || current.secondary_normalized > 1.0F) return 17;
        if (current.tempo_bpm < 20.0F || current.tempo_bpm > 300.0F) return 18;
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 19;

    return 0;
}
