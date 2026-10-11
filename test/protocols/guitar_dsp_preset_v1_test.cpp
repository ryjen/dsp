#include <dsp/protocols/guitar_dsp_preset_v1.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <iostream>
#include <string_view>

namespace {
int fail(int code) {
    std::cerr << "failure-code=" << code << "\n";
    return code;
}

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
    if (set_preset_name(preset, "jazz-blues") != ProtocolError::none) return fail(1);
    preset.tempo_bpm = 92;
    preset.feedback_milli = 320;
    preset.free_ms = 375;
    preset.mix_milli = 500;
    preset.subdivision = PresetSubdivisionV1::dotted_eighth;

    std::array<char, 256> buffer{};
    const auto encoded = serialize_preset_v1(preset, buffer);
    if (encoded.error != ProtocolError::none) return fail(2);
    if (std::string_view{buffer.data(), encoded.size} != expected) return fail(3);

    DelayPresetV1 maximum;
    if (set_preset_name(maximum, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") != ProtocolError::none) return fail(4);
    maximum.tempo_bpm = 300;
    maximum.feedback_milli = 950;
    maximum.free_ms = 2000;
    maximum.mix_milli = 1000;
    maximum.subdivision = PresetSubdivisionV1::dotted_eighth;
    if (serialize_preset_v1(maximum, buffer).error != ProtocolError::none) return fail(5);

    DelayPresetV1 invalid_name;
    if (set_preset_name(invalid_name, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") != ProtocolError::invalid_preset) return fail(6);
    if (set_preset_name(invalid_name, "Jazz Blues") != ProtocolError::invalid_preset) return fail(7);

    auto invalid = preset;
    invalid.tempo_bpm = 19;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_tempo) return fail(8);
    invalid = preset;
    invalid.tempo_bpm = 301;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_tempo) return fail(9);
    invalid = preset;
    invalid.feedback_milli = 951;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_feedback) return fail(10);
    invalid = preset;
    invalid.free_ms = 0;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_free_ms) return fail(11);
    invalid = preset;
    invalid.free_ms = 2001;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_free_ms) return fail(12);
    invalid = preset;
    invalid.mix_milli = 1001;
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_mix) return fail(13);
    invalid = preset;
    invalid.subdivision = static_cast<PresetSubdivisionV1>(255);
    if (serialize_preset_v1(invalid, buffer).error != ProtocolError::invalid_subdivision) return fail(14);

    std::array<char, 32> small{};
    if (serialize_preset_v1(preset, small).error != ProtocolError::too_large) return fail(15);

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000; ++i) {
        const auto result = serialize_preset_v1(preset, buffer);
        if (result.error != ProtocolError::none || result.size != expected.size()) return fail(16);
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return fail(17);


    const auto parsed = parse_preset_v1(expected);
    if (parsed.error != ProtocolError::none) return fail(18);
    if (parsed.value.preset_name() != "jazz-blues") return fail(19);
    if (parsed.value.tempo_bpm != 92U) return fail(20);
    if (parsed.value.feedback_milli != 320U) return fail(21);
    if (parsed.value.free_ms != 375U) return fail(22);
    if (parsed.value.mix_milli != 500U) return fail(23);
    if (parsed.value.subdivision != PresetSubdivisionV1::dotted_eighth) return fail(24);

    const auto reparsed_encoded = serialize_preset_v1(parsed.value, buffer);
    if (reparsed_encoded.error != ProtocolError::none) return fail(25);
    if (std::string_view{buffer.data(), reparsed_encoded.size} != expected) return fail(26);

    std::array<char, 257> oversized{};
    oversized.fill('x');
    if (parse_preset_v1(std::string_view{oversized.data(), oversized.size()}).error !=
        ProtocolError::too_large) return fail(27);

    constexpr std::string_view whitespace =
        R"({ "schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(whitespace).error != ProtocolError::non_canonical) return fail(28);

    constexpr std::string_view reordered =
        R"({"version":1,"schema":"guitar-dsp.preset","preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(reordered).error != ProtocolError::non_canonical) return fail(29);

    constexpr std::string_view wrong_schema =
        R"({"schema":"other","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(wrong_schema).error != ProtocolError::wrong_schema) return fail(30);

    constexpr std::string_view wrong_version =
        R"({"schema":"guitar-dsp.preset","version":2,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(wrong_version).error != ProtocolError::unsupported_version) return fail(31);

    constexpr std::string_view bad_preset =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"Jazz Blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(bad_preset).error != ProtocolError::invalid_preset) return fail(32);

    constexpr std::string_view low_tempo =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":19,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    constexpr std::string_view high_tempo =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":301,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(low_tempo).error != ProtocolError::invalid_tempo) return fail(33);
    if (parse_preset_v1(high_tempo).error != ProtocolError::invalid_tempo) return fail(34);

    constexpr std::string_view high_feedback =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":951,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(high_feedback).error != ProtocolError::invalid_feedback) return fail(35);

    constexpr std::string_view low_free =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":0,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    constexpr std::string_view high_free =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":2001,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(low_free).error != ProtocolError::invalid_free_ms) return fail(36);
    if (parse_preset_v1(high_free).error != ProtocolError::invalid_free_ms) return fail(37);

    constexpr std::string_view high_mix =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":1001,"delay.subdivision":"dotted-eighth"}})";
    if (parse_preset_v1(high_mix).error != ProtocolError::invalid_mix) return fail(38);

    constexpr std::string_view bad_subdivision =
        R"({"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"triplet"}})";
    if (parse_preset_v1(bad_subdivision).error != ProtocolError::invalid_subdivision) return fail(39);

    if (parse_preset_v1(expected.substr(0, expected.size() - 1U)).error !=
        ProtocolError::non_canonical) return fail(40);

    allocations.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t i = 0; i < 10000; ++i) {
        const auto result = parse_preset_v1(expected);
        if (result.error != ProtocolError::none ||
            result.value.tempo_bpm != 92U) return fail(41);
    }
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return fail(42);

    return fail(0);
}
