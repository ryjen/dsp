#include <dsp/targets/daisy/tempo_controller.hpp>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};

bool near(float actual, float expected, float tolerance = 0.1F) {
    return std::abs(actual - expected) <= tolerance;
}
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
    using dsp::targets::daisy::TempoController;

    TempoController midi;
    if (midi.tempo_bpm() != 120.0F || midi.midi_authoritative()) return 1;

    midi.on_midi_start(0);
    if (midi.midi_authoritative()) return 2;

    constexpr std::uint64_t first_clock = 100000;
    constexpr std::uint64_t interval_120 = 20833;
    for (std::uint64_t i = 0; i < 24; ++i) {
        midi.on_midi_clock(first_clock + i * interval_120);
    }
    if (!midi.midi_authoritative()) return 3;
    if (!near(midi.tempo_bpm(), 120.0F)) return 4;

    const float stable_bpm = midi.tempo_bpm();
    midi.on_midi_clock(first_clock + 22 * interval_120);
    if (midi.tempo_bpm() != stable_bpm) return 5;

    const auto last_clock = first_clock + 23 * interval_120;
    midi.on_midi_stop();
    if (midi.midi_authoritative()) return 6;
    midi.on_midi_continue(last_clock + 1000);
    if (!midi.midi_authoritative()) return 7;

    TempoController slow;
    slow.on_midi_start(0);
    slow.on_midi_clock(1000);
    slow.on_midi_clock(126000);
    if (!slow.midi_authoritative() || !near(slow.tempo_bpm(), 20.0F)) return 8;
    slow.update_timeout(126000 + 750000);
    if (!slow.midi_authoritative()) return 9;
    slow.update_timeout(126000 + 750001);
    if (slow.midi_authoritative()) return 10;

    TempoController taps;
    taps.on_tap(1000000);
    taps.on_tap(1500000);
    if (!near(taps.tempo_bpm(), 120.0F)) return 11;

    TempoController invalid_taps;
    invalid_taps.on_tap(1000000);
    invalid_taps.on_tap(1199000);
    if (invalid_taps.tempo_bpm() != 120.0F) return 12;
    invalid_taps.on_tap(4001001);
    if (invalid_taps.tempo_bpm() != 120.0F) return 13;

    TempoController averaged;
    averaged.on_tap(1000000);
    averaged.on_tap(1500000);
    averaged.on_tap(2000000);
    averaged.on_tap(2500000);
    averaged.on_tap(3000000);
    averaged.on_tap(3600000);
    if (!near(averaged.tempo_bpm(), 114.2857F)) return 14;

    TempoController priority;
    priority.on_midi_start(0);
    priority.on_midi_clock(100000);
    priority.on_midi_clock(120833);
    if (!priority.midi_authoritative() || !near(priority.tempo_bpm(), 120.0F)) return 15;
    priority.on_tap(1000000);
    priority.on_tap(1600000);
    if (!near(priority.tempo_bpm(), 120.0F)) return 16;
    priority.on_midi_stop();
    priority.on_tap(2000000);
    priority.on_tap(2600000);
    if (!near(priority.tempo_bpm(), 100.0F)) return 17;

    TempoController bounded;
    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    std::uint64_t now = 1000000;
    bounded.on_midi_start(now);
    for (std::size_t i = 0; i < 10000; ++i) {
        now += interval_120;
        bounded.on_midi_clock(now);
        bounded.update_timeout(now);
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 18;

    return 0;
}
