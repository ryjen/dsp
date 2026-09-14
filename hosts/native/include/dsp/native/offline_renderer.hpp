#pragma once

#include <dsp/processor.hpp>

#include <cstddef>
#include <vector>

namespace dsp::native {

enum class RenderError {
    none,
    invalid_spec,
    invalid_block_size,
    invalid_input,
    prepare_failed,
};

struct RenderResult {
    RenderError error{RenderError::none};
    std::vector<std::vector<float>> channels;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == RenderError::none;
    }
};

[[nodiscard]] RenderResult render_offline(
    Processor& processor,
    const ProcessSpec& spec,
    const std::vector<std::vector<float>>& input,
    std::size_t block_size);

} // namespace dsp::native
