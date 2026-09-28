#include <dsp/targets/daisy/midi_adapter.hpp>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};

bool near(float actual, float expected, float tolerance = 1.0e-6F) {
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
    using namespace dsp::targets::daisy;

    MidiAdapter adapter;

    auto event = adapter.translate(
        MidiMessage{MidiMessageKind::control_change, 1, 20, 127, true}, 1000);
    if (!event || event->kind != ControlEventKind::primary_control) return 1;
    if (!near(event->normalized, 1.0F)) return 2;

    event = adapter.translate(
        MidiMessage{MidiMessageKind::control_change, 1, 21, 64, true}, 2000);
    if (!event || event->kind != ControlEventKind::secondary_control) return 3;
    if (!near(event->normalized, 64.0F / 127.0F)) return 4;

    if (adapter.translate(
            MidiMessage{MidiMessageKind::control_change, 2, 20, 127, true}, 3000)) return 5;
    if (adapter.translate(
            MidiMessage{MidiMessageKind::control_change, 1, 22, 127, true}, 4000)) return 6;
    if (adapter.translate(
            MidiMessage{MidiMessageKind::unsupported, 1, 0, 0, true}, 5000)) return 7;
    if (adapter.translate(
            MidiMessage{MidiMessageKind::control_change, 1, 20, 127, false}, 6000)) return 8;

    event = adapter.translate(MidiMessage{MidiMessageKind::clock, 0, 0, 0, true}, 7000);
    if (!event || event->kind != ControlEventKind::midi_clock || event->timestamp_us != 7000) return 9;

    event = adapter.translate(MidiMessage{MidiMessageKind::start, 0, 0, 0, true}, 8000);
    if (!event || event->kind != ControlEventKind::transport_start) return 10;
    event = adapter.translate(MidiMessage{MidiMessageKind::stop, 0, 0, 0, true}, 9000);
    if (!event || event->kind != ControlEventKind::transport_stop) return 11;
    event = adapter.translate(
        MidiMessage{MidiMessageKind::continue_playback, 0, 0, 0, true}, 10000);
    if (!event || event->kind != ControlEventKind::transport_continue) return 12;

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000; ++i) {
        const auto value = static_cast<std::uint8_t>(i % 128U);
        const auto translated = adapter.translate(
            MidiMessage{MidiMessageKind::control_change, 1, 20, value, true},
            static_cast<MonotonicMicros>(i));
        if (!translated) return 13;
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 14;

    return 0;
}
