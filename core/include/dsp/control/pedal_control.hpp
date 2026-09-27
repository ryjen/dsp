#pragma once

#include <cstdint>

namespace dsp::control {

enum class PedalMode : std::uint8_t {
    free = 0,
    quarter = 1,
    dotted_eighth = 2,
};

struct PedalControlSnapshot {
    float primary_normalized{0.0F};
    float secondary_normalized{0.0F};
    float tempo_bpm{120.0F};
    PedalMode mode{PedalMode::free};
    bool bypassed{false};
    bool transport_running{false};
};

}  // namespace dsp::control
