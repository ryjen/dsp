#pragma once

#include <dsp/atomic_float.hpp>
#include <dsp/processor.hpp>

#include <memory>

namespace dsp::effects {

class TremoloProcessor final : public Processor {
public:
    TremoloProcessor();
    ~TremoloProcessor() override;

    TremoloProcessor(const TremoloProcessor&) = delete;
    TremoloProcessor& operator=(const TremoloProcessor&) = delete;
    TremoloProcessor(TremoloProcessor&&) = delete;
    TremoloProcessor& operator=(TremoloProcessor&&) = delete;

    bool prepare(const ProcessSpec& spec) override;
    void reset() noexcept override;
    void process(AudioBlock block) noexcept override;

    void set_rate_hz(float value) noexcept;
    void set_depth(float value) noexcept;
    void set_shape(float value) noexcept;

    [[nodiscard]] float rate_hz() const noexcept;
    [[nodiscard]] float depth() const noexcept;
    [[nodiscard]] float shape() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    AtomicFloat rate_hz_{4.0F};
    AtomicFloat depth_{0.5F};
    AtomicFloat shape_{0.0F};
};

}  // namespace dsp::effects
