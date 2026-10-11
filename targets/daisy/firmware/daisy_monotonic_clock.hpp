#pragma once

#include "sys/system.h"

#include <dsp/targets/daisy/control_event.hpp>

namespace dsp::targets::daisy::firmware {

class DaisyMonotonicClock {
public:
    [[nodiscard]] MonotonicMicros now() const noexcept {
        return ::daisy::System::GetUs();
    }
};

}  // namespace dsp::targets::daisy::firmware
