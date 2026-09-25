#include <dsp/effects/delay.hpp>
#include <dsp/native/callback_budget.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
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

bool valid_report(const dsp::native::CallbackBudgetResult& result) {
    if (!result) return false;
    const auto& report = result.report;
    return report.callback_count == 2000 &&
           std::isfinite(report.average_seconds) &&
           report.average_seconds >= 0.0 &&
           std::isfinite(report.deadline_seconds) &&
           report.deadline_seconds > 0.0 &&
           std::isfinite(report.average_deadline_fraction) &&
           report.average_deadline_fraction >= 0.0;
}

void configure(dsp::effects::DelayProcessor& processor) {
    processor.set_delay_ms(375.0F);
    processor.set_feedback(0.65F);
    processor.set_mix(0.5F);
}

void print_report(
    const char* label,
    const dsp::native::CallbackBudgetReport& report) {
    std::cout << label
              << " callbacks=" << report.callback_count
              << " avg_us=" << report.average_seconds * 1.0e6
              << " deadline_us=" << report.deadline_seconds * 1.0e6
              << " fraction=" << report.average_deadline_fraction << '\n';
}

int main() {
    dsp::effects::DelayProcessor processor;
    configure(processor);
    if (!processor.prepare({48000.0, 64, 2})) return 1;
    processor.reset();

    std::array<float, 64> left{};
    std::array<float, 64> right{};
    left.fill(1.0F);
    right.fill(1.0F);
    float* channels[]{left.data(), right.data()};
    const dsp::AudioBlock block{channels, 2, 64};

    allocation_count.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    void* probe = ::operator new(16);
    ::operator delete(probe);
    tracking.store(false, std::memory_order_release);
    if (allocation_count.load(std::memory_order_relaxed) != 1) return 2;

    allocation_count.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    for (std::size_t callback = 0; callback < 2000; ++callback) {
        processor.process(block);
    }
    tracking.store(false, std::memory_order_release);
    if (allocation_count.load(std::memory_order_relaxed) != 0) return 3;

    dsp::effects::DelayProcessor budget48;
    configure(budget48);
    const auto report48 =
        dsp::native::measure_callback_budget(
            budget48, {48000.0, 64, 2}, 64, 2000);
    if (!valid_report(report48)) return 4;
    print_report("48k/64/stereo", report48.report);

    dsp::effects::DelayProcessor budget96;
    configure(budget96);
    const auto report96 =
        dsp::native::measure_callback_budget(
            budget96, {96000.0, 128, 2}, 128, 2000);
    if (!valid_report(report96)) return 5;
    print_report("96k/128/stereo", report96.report);

    return 0;
}
