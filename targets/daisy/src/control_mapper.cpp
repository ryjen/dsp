#include <dsp/targets/daisy/control_mapper.hpp>

#include <cmath>

namespace dsp::targets::daisy {
namespace {

bool valid_normalized(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

}  // namespace

ControlMapper::ControlMapper(
    dsp::control::PedalControlState& state,
    TempoController& tempo) noexcept
    : state_{state}, tempo_{tempo} {
    const auto current = state_.snapshot();
    last_primary_ = current.primary_normalized;
    last_secondary_ = current.secondary_normalized;
    toggle1_.stable = static_cast<std::uint8_t>(current.mode);
}

void ControlMapper::update_pot1(float normalized) noexcept {
    publish_primary(normalized, true);
}

void ControlMapper::update_pot2(float normalized) noexcept {
    publish_secondary(normalized, true);
}

void ControlMapper::update_toggle1(
    std::uint8_t position,
    MonotonicMicros now) noexcept {
    if (position > 2U) return;
    if (!toggle1_.update(position, now)) return;
    publish_mode(static_cast<dsp::control::PedalMode>(toggle1_.stable));
}

void ControlMapper::update_footswitch1(
    bool pressed,
    MonotonicMicros now) noexcept {
    if (!footswitch1_.update(pressed, now)) return;
    if (footswitch1_.stable) {
        publish_bypass_toggle();
    }
}

void ControlMapper::update_footswitch2(
    bool pressed,
    MonotonicMicros now) noexcept {
    if (!footswitch2_.update(pressed, now)) return;
    if (footswitch2_.stable) {
        tempo_.on_tap(now);
        publish_tempo();
    }
}

void ControlMapper::apply_midi_control(
    std::uint8_t cc,
    float normalized) noexcept {
    if (cc == 20U) {
        publish_primary(normalized, false);
    } else if (cc == 21U) {
        publish_secondary(normalized, false);
    }
}

dsp::control::PedalControlSnapshot ControlMapper::snapshot() const noexcept {
    return state_.snapshot();
}

void ControlMapper::publish_primary(
    float normalized,
    bool apply_deadband) noexcept {
    if (!valid_normalized(normalized)) return;
    if (apply_deadband &&
        std::abs(normalized - last_primary_) < pot_deadband) {
        return;
    }

    last_primary_ = normalized;
    auto next = state_.snapshot();
    next.primary_normalized = normalized;
    state_.publish(next);
}

void ControlMapper::publish_secondary(
    float normalized,
    bool apply_deadband) noexcept {
    if (!valid_normalized(normalized)) return;
    if (apply_deadband &&
        std::abs(normalized - last_secondary_) < pot_deadband) {
        return;
    }

    last_secondary_ = normalized;
    auto next = state_.snapshot();
    next.secondary_normalized = normalized;
    state_.publish(next);
}

void ControlMapper::publish_mode(dsp::control::PedalMode mode) noexcept {
    auto next = state_.snapshot();
    next.mode = mode;
    state_.publish(next);
}

void ControlMapper::publish_bypass_toggle() noexcept {
    auto next = state_.snapshot();
    next.bypassed = !next.bypassed;
    state_.publish(next);
}

void ControlMapper::publish_tempo() noexcept {
    auto next = state_.snapshot();
    next.tempo_bpm = tempo_.tempo_bpm();
    state_.publish(next);
}

}  // namespace dsp::targets::daisy
