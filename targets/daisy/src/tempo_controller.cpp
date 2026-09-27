#include <dsp/targets/daisy/tempo_controller.hpp>

#include <algorithm>
#include <cstdint>

namespace dsp::targets::daisy {
namespace {

constexpr double micros_per_minute = 60000000.0;
constexpr double midi_clocks_per_quarter = 24.0;
constexpr MonotonicMicros minimum_clock_timeout = 250000;
constexpr MonotonicMicros minimum_tap_interval = 200000;
constexpr MonotonicMicros maximum_tap_interval = 3000000;

template <std::size_t N>
double average_prefix(
    const std::array<MonotonicMicros, N>& values,
    std::size_t count) noexcept {
    if (count == 0U) return 0.0;
    long double total = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        total += static_cast<long double>(values[i]);
    }
    return static_cast<double>(total / static_cast<long double>(count));
}

float clamp_bpm(double bpm) noexcept {
    return static_cast<float>(std::clamp(bpm, 20.0, 300.0));
}

}  // namespace

void TempoController::on_midi_start(MonotonicMicros now) noexcept {
    midi_running_ = true;
    midi_authoritative_ = has_midi_estimate_ && clock_is_fresh(now);
}

void TempoController::on_midi_continue(MonotonicMicros now) noexcept {
    on_midi_start(now);
}

void TempoController::on_midi_stop() noexcept {
    midi_running_ = false;
    midi_authoritative_ = false;
}

void TempoController::on_midi_clock(MonotonicMicros now) noexcept {
    if (!has_last_clock_) {
        last_clock_ = now;
        has_last_clock_ = true;
        return;
    }
    if (now <= last_clock_) return;

    const MonotonicMicros interval = now - last_clock_;
    last_clock_ = now;

    midi_intervals_[midi_interval_index_] = interval;
    midi_interval_index_ = (midi_interval_index_ + 1U) % max_midi_intervals;
    if (midi_interval_count_ < max_midi_intervals) {
        ++midi_interval_count_;
    }

    publish_midi_estimate();
    has_midi_estimate_ = true;
    if (midi_running_) {
        midi_authoritative_ = true;
    }
}

void TempoController::on_tap(MonotonicMicros now) noexcept {
    if (midi_authoritative_) return;

    if (!has_last_tap_) {
        last_tap_ = now;
        has_last_tap_ = true;
        return;
    }
    if (now <= last_tap_) return;

    const MonotonicMicros interval = now - last_tap_;
    if (interval < minimum_tap_interval || interval > maximum_tap_interval) {
        last_tap_ = now;
        tap_interval_count_ = 0;
        tap_interval_index_ = 0;
        return;
    }

    last_tap_ = now;
    tap_intervals_[tap_interval_index_] = interval;
    tap_interval_index_ = (tap_interval_index_ + 1U) % max_tap_intervals;
    if (tap_interval_count_ < max_tap_intervals) {
        ++tap_interval_count_;
    }
    publish_tap_estimate();
}

void TempoController::update_timeout(MonotonicMicros now) noexcept {
    if (!midi_authoritative_ || !midi_running_ || !has_last_clock_) return;
    if (now <= last_clock_) return;
    if (now - last_clock_ > clock_timeout()) {
        midi_authoritative_ = false;
    }
}

float TempoController::tempo_bpm() const noexcept {
    return tempo_bpm_;
}

bool TempoController::midi_authoritative() const noexcept {
    return midi_authoritative_;
}

bool TempoController::clock_is_fresh(MonotonicMicros now) const noexcept {
    if (!has_last_clock_ || now < last_clock_) return false;
    return now - last_clock_ <= clock_timeout();
}

MonotonicMicros TempoController::clock_timeout() const noexcept {
    if (!has_midi_estimate_ || midi_interval_count_ == 0U) {
        return minimum_clock_timeout;
    }
    const double expected = average_prefix(midi_intervals_, midi_interval_count_);
    const auto six_intervals =
        static_cast<MonotonicMicros>(expected * 6.0);
    return std::max(minimum_clock_timeout, six_intervals);
}

void TempoController::publish_midi_estimate() noexcept {
    const double average =
        average_prefix(midi_intervals_, midi_interval_count_);
    if (average <= 0.0) return;
    tempo_bpm_ =
        clamp_bpm(micros_per_minute / (average * midi_clocks_per_quarter));
}

void TempoController::publish_tap_estimate() noexcept {
    const double average =
        average_prefix(tap_intervals_, tap_interval_count_);
    if (average <= 0.0) return;
    tempo_bpm_ = clamp_bpm(micros_per_minute / average);
}

}  // namespace dsp::targets::daisy
