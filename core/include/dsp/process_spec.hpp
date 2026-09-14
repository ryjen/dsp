#pragma once

#include <cstddef>

namespace dsp {

struct ProcessSpec {
    double sample_rate{};
    std::size_t max_block_size{};
    std::size_t channel_count{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return sample_rate > 0.0 && max_block_size > 0 && channel_count > 0;
    }
};

} // namespace dsp
