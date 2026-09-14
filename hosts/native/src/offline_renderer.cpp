#include <dsp/native/offline_renderer.hpp>

#include <algorithm>

namespace dsp::native {

RenderResult render_offline(
    Processor& processor,
    const ProcessSpec& spec,
    const std::vector<std::vector<float>>& input,
    std::size_t block_size) {
    if (!spec.valid()) return {RenderError::invalid_spec, {}};
    if (block_size == 0 || block_size > spec.max_block_size) {
        return {RenderError::invalid_block_size, {}};
    }
    if (input.size() != spec.channel_count) return {RenderError::invalid_input, {}};

    const std::size_t frames = input.empty() ? 0 : input.front().size();
    for (const auto& channel : input) {
        if (channel.size() != frames) return {RenderError::invalid_input, {}};
    }

    if (!processor.prepare(spec)) return {RenderError::prepare_failed, {}};
    processor.reset();

    std::vector<std::vector<float>> output = input;
    std::vector<float*> channel_pointers(spec.channel_count, nullptr);

    for (std::size_t offset = 0; offset < frames; offset += block_size) {
        const std::size_t block_frames = std::min(block_size, frames - offset);
        for (std::size_t channel = 0; channel < spec.channel_count; ++channel) {
            channel_pointers[channel] = output[channel].data() + offset;
        }
        processor.process(AudioBlock{channel_pointers.data(), spec.channel_count, block_frames});
    }

    return {RenderError::none, std::move(output)};
}

} // namespace dsp::native
