#include <dsp/targets/daisy/realtime_midi_inbox.hpp>

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
    using namespace dsp::targets::daisy;

    RealtimeMidiInbox inbox;
    const MidiMessage clock{
        MidiMessageKind::clock, 0U, 0U, 0U, true};

    for (std::uint32_t i = 0; i < RealtimeMidiInbox::capacity; ++i) {
        if (!inbox.push(clock, static_cast<MonotonicMicros>(i))) return 1;
    }
    if (inbox.push(clock, 999U)) return 2;

    for (std::uint32_t i = 0; i < RealtimeMidiInbox::capacity; ++i) {
        const auto item = inbox.pop();
        if (!item) return 3;
        if (item->message.kind != MidiMessageKind::clock) return 4;
        if (item->timestamp != static_cast<MonotonicMicros>(i)) return 5;
    }
    if (inbox.pop()) return 6;

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::uint32_t i = 0; i < 10000U; ++i) {
        if (!inbox.push(clock, static_cast<MonotonicMicros>(i))) return 7;
        const auto item = inbox.pop();
        if (!item || item->timestamp != static_cast<MonotonicMicros>(i)) {
            return 8;
        }
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 9;

    return 0;
}
