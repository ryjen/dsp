#pragma once

#include <dsp/linear_smoother.hpp>
#include <dsp/process_spec.hpp>
#include <dsp/processor.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsp::targets::daisy {

class PedalRuntime {
public:
    bool prepare(const dsp::ProcessSpec& spec, dsp::Processor& processor);

    void set_bypassed(bool bypassed) noexcept;

    void process(
        const float* const* input,
        float* const* output,
        std::size_t channels,
        std::size_t frames) noexcept;

private:
    [[nodiscard]] std::size_t bypass_ramp_samples() const noexcept;

    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    dsp::Processor* processor_{nullptr};
    dsp::ProcessSpec spec_{};
    std::vector<float> dry_scratch_;
    std::vector<float*> process_channels_;
    std::atomic<std::uint32_t> bypassed_{0U};
    dsp::LinearSmoother bypass_mix_;
    float last_bypass_target_{0.0F};
    bool prepared_{false};
};

}  // namespace dsp::targets::daisy
