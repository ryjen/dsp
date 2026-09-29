#pragma once

#include "sys/system.h"

#include <dsp/targets/daisy/control_event.hpp>

#include <cstdint>

namespace dsp::targets::daisy::firmware {

class DaisyMonotonicClock {
public:
    [[nodiscard]] MonotonicMicros now() noexcept {
        const std::uint32_t current = ::daisy::System::GetUs();
        if (initialized_ && current < last_) {
            epoch_ += (MonotonicMicros{1} << 32U);
        }
        last_ = current;
        initialized_ = true;
        return epoch_ + current;
    }

private:
    std::uint32_t last_{0};
    MonotonicMicros epoch_{0};
    bool initialized_{false};
};

}  // namespace dsp::targets::daisy::firmware
