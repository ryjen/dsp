#include <dsp/protocols/guitar_dsp_preset_v1.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string_view>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};

constexpr std::string_view expected =
    R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
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
    using namespace dsp::protocols;

    DelayPresetV1 preset;
    if (set_preset_name(preset, "jazz-blues") != ProtocolError::none) return 1;
    preset.tempo_bpm = 92;
    preset.feedback_milli = 320;
    preset.free_ms = 375;
    preset.mix_milli = 500;
    preset.subdivision = PresetSubdivisionV1::dotted_eighth;

    std::array<char, 256> buffer{};
    const auto encoded = serialize_preset_v1(preset, buffer);
    if (encoded.error != ProtocolError::none) return 2;
    if (std::string_view{buffer.data(), encoded.size} != expected) return 3;

    DelayPresetV1 maximum;
    if (set_preset_name(maximum, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") != ProtocolError::none) return 4;
    maximum.tempo_bpm = 300;
    maximum.feedback_milli = 950;
    maximum.free_ms = 2000;
    maximum.mix_milli = 1000;
    maximum.subdivision = PresetSubdivisionV1::dotted_eighth;
    if (serialize_preset_v1(maximum, buffer).error != ProtocolError::none) return 5;

    DelayPresetV1 invalid_name;
    if (set_preset_name(invalid_name, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") != ProtocolError::invalid_preset) return 6;
    if (set_preset_name(invalid_name, "Jazz Blues") != ProtocolError::invalid_preset) return 7;

    auto invalid = preset;
    invalid.tempo_bpm = 19;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_tempo) return 8;
    invalid = preset;
    invalid.tempo_bpm = 301;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_tempo) return 9;
    invalid = preset;
    invalid.feedback_milli = 951;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_feedback) return 10;
    invalid = preset;
    invalid.free_ms = 0;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_free_ms) return 11;
    invalid = preset;
    invalid.free_ms = 2001;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_free_ms) return 12;
    invalid = preset;
    invalid.mix_milli = 1001;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_mix) return 13;
    invalid = preset;
    invalid.subdivision = static_cast<PresetSubdivisionV1>(255);
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_subdivision) return 14;

    std::array<char, 32> small{};
    if (serialize_preset_v1(preset, small).error != ProtocolError::too_large) return 15;

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000; ++i) {
        const auto result = serialize_preset_v1(preset, buffer);
        if (result.error != ProtocolError::none || result.size != expected.size()) return 16;
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 17;

    return 0;
}
