#include <dsp/core.hpp>

#include <cmath>

int main() {
    dsp::AtomicFloat parameter{0.25F};
    if (parameter.load() != 0.25F) return 1;
    parameter.store(0.75F);
    if (parameter.load() != 0.75F) return 2;

    dsp::LinearSmoother smoother;
    smoother.reset(0.0F);
    smoother.set_target(1.0F, 4);
    const float expected[]{0.25F, 0.5F, 0.75F, 1.0F};
    for (float value : expected) {
        if (std::abs(smoother.next() - value) > 1.0e-6F) return 3;
    }
    if (smoother.smoothing()) return 4;
    if (std::abs(smoother.value() - 1.0F) > 1.0e-6F) return 5;

    smoother.set_target(0.5F, 0);
    if (smoother.smoothing()) return 6;
    if (std::abs(smoother.value() - 0.5F) > 1.0e-6F) return 7;
    return 0;
}
