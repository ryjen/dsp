#pragma once

#include <atomic>
#include <bit>
#include <cstdint>

namespace dsp {

class AtomicFloat {
public:
    explicit AtomicFloat(float initial = 0.0F) noexcept
        : bits_{std::bit_cast<std::uint32_t>(initial)} {}

    void store(float value) noexcept {
        bits_.store(std::bit_cast<std::uint32_t>(value), std::memory_order_release);
    }

    [[nodiscard]] float load() const noexcept {
        return std::bit_cast<float>(bits_.load(std::memory_order_acquire));
    }

private:
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    std::atomic<std::uint32_t> bits_;
};

} // namespace dsp
