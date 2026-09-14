#include <dsp/core.hpp>
#include <dsp/native/callback_budget.hpp>

#include <cmath>
#include <iostream>

class NoOpProcessor final : public dsp::Processor {
public:
    bool prepare(const dsp::ProcessSpec& spec) override { return spec.valid() && allow_prepare; }
    void reset() noexcept override {}
    void process(dsp::AudioBlock) noexcept override { ++callbacks; }

    bool allow_prepare{true};
    std::size_t callbacks{};
};

int main() {
    NoOpProcessor processor;
    const dsp::ProcessSpec spec{48000.0, 64, 2};

    if (dsp::native::measure_callback_budget(processor, dsp::ProcessSpec{}, 64, 10)) return 1;
    if (dsp::native::measure_callback_budget(processor, spec, 0, 10)) return 2;
    if (dsp::native::measure_callback_budget(processor, spec, 65, 10)) return 3;
    if (dsp::native::measure_callback_budget(processor, spec, 64, 0)) return 4;

    const auto result = dsp::native::measure_callback_budget(processor, spec, 64, 1000);
    if (!result) return 5;
    if (result.report.callback_count != 1000 || processor.callbacks != 1000) return 6;

    const double expected_deadline = 64.0 / 48000.0;
    if (std::abs(result.report.deadline_seconds - expected_deadline) > 1.0e-12) return 7;
    if (result.report.elapsed_seconds < 0.0 || result.report.average_seconds < 0.0) return 8;
    if (result.report.average_deadline_fraction < 0.0 || !std::isfinite(result.report.average_deadline_fraction)) return 9;

    NoOpProcessor rejected;
    rejected.allow_prepare = false;
    if (dsp::native::measure_callback_budget(rejected, spec, 64, 10)) return 10;

    std::cout << "callbacks=" << result.report.callback_count
              << " avg_us=" << result.report.average_seconds * 1.0e6
              << " deadline_us=" << result.report.deadline_seconds * 1.0e6
              << " fraction=" << result.report.average_deadline_fraction << '\n';
    return 0;
}
