#pragma once

#include <cstddef>

namespace dsp {

class LinearSmoother {
public:
    void reset(float value) noexcept {
        current_ = value;
        target_ = value;
        step_ = 0.0F;
        remaining_ = 0;
    }

    void set_target(float target, std::size_t samples) noexcept {
        if (samples == 0) {
            reset(target);
            return;
        }
        target_ = target;
        remaining_ = samples;
        step_ = (target_ - current_) / static_cast<float>(samples);
    }

    [[nodiscard]] float next() noexcept {
        if (remaining_ == 0) return current_;
        current_ += step_;
        --remaining_;
        if (remaining_ == 0) current_ = target_;
        return current_;
    }

    [[nodiscard]] float value() const noexcept { return current_; }
    [[nodiscard]] bool smoothing() const noexcept { return remaining_ != 0; }

private:
    float current_{};
    float target_{};
    float step_{};
    std::size_t remaining_{};
};

} // namespace dsp
