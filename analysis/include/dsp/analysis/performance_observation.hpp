#pragma once

#include <cstdint>

namespace dsp::analysis {

inline constexpr std::uint8_t performance_observation_version = 1U;

enum class ObservationQualityFlag : std::uint8_t {
    clipped = 1U << 0U,
    short_event = 1U << 1U,
    non_finite_input = 1U << 2U,
};

[[nodiscard]] constexpr std::uint8_t quality_mask(
    ObservationQualityFlag flag) noexcept {
    return static_cast<std::uint8_t>(flag);
}

struct PerformanceObservationV1 {
    std::uint8_t version{performance_observation_version};
    std::uint8_t quality_flags{};
    std::uint8_t has_inter_event_spacing{};
    std::uint8_t reserved{};
    std::uint64_t start_frame{};
    std::uint64_t end_frame{};
    std::uint64_t timestamp_ms{};
    std::uint64_t attack_ms{};
    std::uint64_t duration_ms{};
    std::uint64_t inter_event_spacing_ms{};
    std::uint16_t peak_milli{};

    [[nodiscard]] bool reliable() const noexcept {
        return quality_flags == 0U;
    }

    [[nodiscard]] bool has_quality_flag(
        ObservationQualityFlag flag) const noexcept {
        return (quality_flags & quality_mask(flag)) != 0U;
    }
};

}  // namespace dsp::analysis
