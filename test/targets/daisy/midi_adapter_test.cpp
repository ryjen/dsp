#include <dsp/targets/daisy/midi_adapter.hpp>

#include <atomic>
#include <cmath>
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
    using namespace dsp::targets::daisy;

    MidiAdapter adapter;

    auto event = adapter.translate({MidiMessageKind::clock, 0, 0, 0, true}, 101);
    if (!event || event->kind != ControlEventKind::midi_clock || event->timestamp != 101) return 1;

    event = adapter.translate({MidiMessageKind::start, 0, 0, 0, true}, 102);
    if (!event || event->kind != ControlEventKind::transport_start) return 2;
    event = adapter.translate({MidiMessageKind::stop, 0, 0, 0, true}, 103);
    if (!event || event->kind != ControlEventKind::transport_stop) return 3;
    event = adapter.translate({MidiMessageKind::continue_playback, 0, 0, 0, true}, 104);
    if (!event || event->kind != ControlEventKind::transport_continue) return 4;

    event = adapter.translate({MidiMessageKind::control_change, 1, 20, 127, true}, 105);
    if (!event || event->kind != ControlEventKind::primary_normalized) return 5;
    if (event->normalized != 1.0F) return 6;

    event = adapter.translate({MidiMessageKind::control_change, 1, 21, 64, true}, 106);
    if (!event || event->kind != ControlEventKind::secondary_normalized) return 7;
    if (std::abs(event->normalized - (64.0F / 127.0F)) > 1.0e-6F) return 8;

    if (adapter.translate({MidiMessageKind::control_change, 2, 20, 127, true}, 107)) return 9;
    if (adapter.translate({MidiMessageKind::control_change, 0, 20, 127, true}, 108)) return 10;
    if (adapter.translate({MidiMessageKind::control_change, 1, 19, 127, true}, 109)) return 11;
    if (adapter.translate({MidiMessageKind::control_change, 1, 20, 128, true}, 110)) return 12;
    if (adapter.translate({MidiMessageKind::program_change, 1, 0, 0, true}, 111)) return 13;
    if (adapter.translate({MidiMessageKind::unsupported, 0, 0, 0, true}, 112)) return 14;
    if (adapter.translate({MidiMessageKind::control_change, 1, 20, 64, false}, 113)) return 15;

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000; ++i) {
        const auto value = static_cast<std::uint8_t>(i % 128U);
        const auto translated =
            adapter.translate({MidiMessageKind::control_change, 1, 20, value, true}, i);
        if (!translated) return 16;
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 17;

    return 0;
}
