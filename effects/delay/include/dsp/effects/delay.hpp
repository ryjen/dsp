#pragma once

#include <dsp/atomic_float.hpp>
#include <dsp/effects/delay_time.hpp>
#include <dsp/processor.hpp>

#include <atomic>
#include <cstdint>
#include <memory>

namespace dsp::effects {

class DelayProcessor final : public Processor {
public:
    static constexpr float max_delay_ms = 2000.0F;
    static constexpr float max_feedback = 0.95F;

    DelayProcessor();
    ~DelayProcessor() override;

    DelayProcessor(const DelayProcessor&) = delete;
    DelayProcessor& operator=(const DelayProcessor&) = delete;
    DelayProcessor(DelayProcessor&&) = delete;
    DelayProcessor& operator=(DelayProcessor&&) = delete;

    bool prepare(const ProcessSpec& spec) override;
    void reset() noexcept override;
    void process(AudioBlock block) noexcept override;

    void set_delay_ms(float value) noexcept;
    void set_feedback(float value) noexcept;
    void set_mix(float value) noexcept;
    void set_tempo_bpm(float value) noexcept;
    void set_subdivision(DelaySubdivision value) noexcept;

    [[nodiscard]] float delay_ms() const noexcept;
    [[nodiscard]] float feedback() const noexcept;
    [[nodiscard]] float mix() const noexcept;
    [[nodiscard]] float tempo_bpm() const noexcept;
    [[nodiscard]] DelaySubdivision subdivision() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    AtomicFloat delay_ms_{250.0F};
    AtomicFloat feedback_{0.0F};
    AtomicFloat mix_{0.5F};
    AtomicFloat tempo_bpm_{120.0F};
    std::atomic<std::uint32_t> subdivision_{
        static_cast<std::uint32_t>(DelaySubdivision::free)};
};

}  // namespace dsp::effects