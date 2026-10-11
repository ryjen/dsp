#pragma once

#include <dsp/protocols/guitar_dsp_preset_v1.hpp>

namespace dsp::effects {
class DelayProcessor;
}

namespace dsp::protocols {

[[nodiscard]] ProtocolError apply_delay_preset_v1(
    const DelayPresetV1& preset,
    dsp::effects::DelayProcessor& processor) noexcept;

}  // namespace dsp::protocols
