#pragma once

#include <dsp/atomic_float.hpp>
#include <dsp/control/pedal_control.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace dsp::control {

class PedalControlState {
public:
    PedalControlState() noexcept = default;

    void publish(const PedalControlSnapshot& snapshot) noexcept {
        publish_finite(primary_normalized_, snapshot.primary_normalized, 0.0F, 1.0F);
        publish_finite(secondary_normalized_, snapshot.secondary_normalized, 0.0F, 1.0F);
        publish_finite(tempo_bpm_, snapshot.tempo_bpm, 20.0F, 300.0F);

        mode_.store(
            static_cast<std::uint32_t>(normalize_mode(snapshot.mode)),
            std::memory_order_release);
        bypassed_.store(snapshot.bypassed ? 1U : 0U, std::memory_order_release);
        transport_running_.store(
            snapshot.transport_running ? 1U : 0U,
            std::memory_order_release);
    }

    [[nodiscard]] PedalControlSnapshot snapshot() const noexcept {
        return {
            primary_normalized_.load(),
            secondary_normalized_.load(),
            tempo_bpm_.load(),
            static_cast<PedalMode>(mode_.load(std::memory_order_acquire)),
            bypassed_.load(std::memory_order_acquire) != 0U,
            transport_running_.load(std::memory_order_acquire) != 0U,
        };
    }

private:
    static void publish_finite(
        AtomicFloat& target,
        float value,
        float minimum,
        float maximum) noexcept {
        if (std::isfinite(value)) {
            target.store(std::clamp(value, minimum, maximum));
        }
    }

    [[nodiscard]] static PedalMode normalize_mode(PedalMode mode) noexcept {
        switch (mode) {
            case PedalMode::free:
            case PedalMode::quarter:
            case PedalMode::dotted_eighth:
                return mode;
        }
        return PedalMode::free;
    }

    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    AtomicFloat primary_normalized_{0.0F};
    AtomicFloat secondary_normalized_{0.0F};
    AtomicFloat tempo_bpm_{120.0F};
    std::atomic<std::uint32_t> mode_{
        static_cast<std::uint32_t>(PedalMode::free)};
    std::atomic<std::uint32_t> bypassed_{0U};
    std::atomic<std::uint32_t> transport_running_{0U};
};

}  // namespace dsp::control
