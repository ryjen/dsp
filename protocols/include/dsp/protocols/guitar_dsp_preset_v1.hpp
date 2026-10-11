#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dsp::protocols {

enum class ProtocolError : std::uint8_t {
    none,
    too_large,
    non_canonical,
    wrong_schema,
    unsupported_version,
    invalid_preset,
    invalid_tempo,
    invalid_feedback,
    invalid_free_ms,
    invalid_mix,
    invalid_subdivision,
};

enum class PresetSubdivisionV1 : std::uint8_t {
    free,
    quarter,
    dotted_eighth,
    eighth,
    eighth_triplet,
    sixteenth,
};

struct DelayPresetV1 {
    std::array<char, 33> preset{};
    std::uint8_t preset_length{};
    std::uint16_t tempo_bpm{120};
    std::uint16_t feedback_milli{};
    std::uint16_t free_ms{250};
    std::uint16_t mix_milli{500};
    PresetSubdivisionV1 subdivision{PresetSubdivisionV1::free};

    [[nodiscard]] std::string_view preset_name() const noexcept {
        return {preset.data(), preset_length};
    }
};

struct SerializeResult {
    ProtocolError error{ProtocolError::none};
    std::size_t size{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == ProtocolError::none;
    }
};

[[nodiscard]] ProtocolError set_preset_name(
    DelayPresetV1& preset,
    std::string_view name) noexcept;

[[nodiscard]] SerializeResult serialize_preset_v1(
    const DelayPresetV1& preset,
    std::span<char> output) noexcept;

}  // namespace dsp::protocols
