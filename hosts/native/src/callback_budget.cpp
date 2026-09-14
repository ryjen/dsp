#include <dsp/native/callback_budget.hpp>

#include <chrono>
#include <vector>

namespace dsp::native {

CallbackBudgetResult measure_callback_budget(
    Processor& processor,
    const ProcessSpec& spec,
    std::size_t frame_count,
    std::size_t callback_count) {
    if (!spec.valid()) return {BudgetError::invalid_spec, {}};
    if (frame_count == 0 || frame_count > spec.max_block_size) {
        return {BudgetError::invalid_frame_count, {}};
    }
    if (callback_count == 0) return {BudgetError::invalid_callback_count, {}};
    if (!processor.prepare(spec)) return {BudgetError::prepare_failed, {}};
    processor.reset();

    std::vector<std::vector<float>> buffers(
        spec.channel_count,
        std::vector<float>(frame_count, 0.0F));
    std::vector<float*> channel_pointers(spec.channel_count, nullptr);
    for (std::size_t channel = 0; channel < spec.channel_count; ++channel) {
        channel_pointers[channel] = buffers[channel].data();
    }

    const AudioBlock block{channel_pointers.data(), spec.channel_count, frame_count};
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t callback = 0; callback < callback_count; ++callback) {
        processor.process(block);
    }
    const auto finish = std::chrono::steady_clock::now();

    const double elapsed = std::chrono::duration<double>(finish - start).count();
    const double average = elapsed / static_cast<double>(callback_count);
    const double deadline = static_cast<double>(frame_count) / spec.sample_rate;

    return {
        BudgetError::none,
        CallbackBudgetReport{
            callback_count,
            elapsed,
            average,
            deadline,
            average / deadline,
        },
    };
}

} // namespace dsp::native
