#pragma once

#include <cstddef>

namespace dsp::targets::daisy::firmware {

inline constexpr double sample_rate_hz = 48000.0;
inline constexpr std::size_t block_size = 48;
inline constexpr std::size_t channel_count = 2;
inline constexpr std::size_t delay_capacity_per_channel = 96002;
inline constexpr std::size_t delay_storage_samples =
    delay_capacity_per_channel * channel_count;

}  // namespace dsp::targets::daisy::firmware
