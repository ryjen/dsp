#pragma once

#include <cstddef>

namespace dsp {

struct AudioBlock {
    float* const* channels{};
    std::size_t channel_count{};
    std::size_t frame_count{};

    [[nodiscard]] float* channel(std::size_t index) const noexcept {
        return channels[index];
    }
};

} // namespace dsp
