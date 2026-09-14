#include <dsp/core.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocation_count{0};
}

void* operator new(std::size_t size) {
    if (tracking.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

class TrackingProcessor final : public dsp::Processor {
public:
    bool prepare(const dsp::ProcessSpec& spec) override {
        if (!spec.valid()) return false;
        smoother_.reset(1.0F);
        return true;
    }

    void reset() noexcept override { smoother_.reset(1.0F); }

    void set_target(float value) noexcept { target_.store(value); }

    void process(dsp::AudioBlock block) noexcept override {
        smoother_.set_target(target_.load(), block.frame_count);
        for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
            const float gain = smoother_.next();
            for (std::size_t channel = 0; channel < block.channel_count; ++channel) {
                block.channel(channel)[frame] *= gain;
            }
        }
    }

private:
    dsp::AtomicFloat target_{1.0F};
    dsp::LinearSmoother smoother_;
};

int main() {
    TrackingProcessor processor;
    const dsp::ProcessSpec spec{48000.0, 64, 2};
    if (!processor.prepare(spec)) return 1;
    processor.set_target(0.5F);

    std::array<float, 64> left{};
    std::array<float, 64> right{};
    left.fill(1.0F);
    right.fill(1.0F);
    float* channels[]{left.data(), right.data()};

    allocation_count.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    void* probe = ::operator new(16);
    ::operator delete(probe);
    tracking.store(false, std::memory_order_release);
    if (allocation_count.load(std::memory_order_relaxed) != 1) return 2;

    allocation_count.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    processor.process(dsp::AudioBlock{channels, 2, 64});
    tracking.store(false, std::memory_order_release);

    if (allocation_count.load(std::memory_order_relaxed) != 0) return 3;
    if (left.back() != 0.5F || right.back() != 0.5F) return 4;
    return 0;
}
