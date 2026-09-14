#pragma once

#include <cmath>
#include <cstddef>

namespace dsp {

struct ProcessSpec {
    double sample_rate{};
    std::size_t max_block_size{};
    std::size_t channel_count{};

    [[nodiscard]] bool valid() const noexcept {
        return std::isfinite(sample_rate) && sample_rate > 0.0 && max_block_size > 0 && channel_count > 0;
    }
};

} // namespace dsp
