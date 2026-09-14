#include <dsp/core.hpp>

#include <array>

int main() {
    dsp::ProcessSpec valid{48000.0, 64, 2};
    if (!valid.valid()) return 1;
    if (dsp::ProcessSpec{0.0, 64, 2}.valid()) return 2;
    if (dsp::ProcessSpec{48000.0, 0, 2}.valid()) return 3;
    if (dsp::ProcessSpec{48000.0, 64, 0}.valid()) return 4;

    std::array<float, 4> left{};
    std::array<float, 4> right{};
    float* channels[]{left.data(), right.data()};
    dsp::AudioBlock block{channels, 2, 4};
    if (block.channel(0) != left.data()) return 5;
    if (block.channel(1) != right.data()) return 6;
    return 0;
}
