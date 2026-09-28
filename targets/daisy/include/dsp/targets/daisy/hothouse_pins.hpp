#pragma once

#include "daisy_seed.h"

namespace dsp::targets::daisy::hothouse {

// Connection facts derived from Cleveland Music Co.'s CC-BY-SA-4.0
// Hothouse open-hardware schematic:
// https://github.com/clevelandmusicco/open-source-pedals
// No HothouseExamples firmware source is copied or linked.
inline constexpr ::daisy::Pin pot1 = ::daisy::seed::D16;
inline constexpr ::daisy::Pin pot2 = ::daisy::seed::D17;
inline constexpr ::daisy::Pin toggle1_up = ::daisy::seed::D9;
inline constexpr ::daisy::Pin toggle1_down = ::daisy::seed::D10;
inline constexpr ::daisy::Pin footswitch1 = ::daisy::seed::D25;
inline constexpr ::daisy::Pin footswitch2 = ::daisy::seed::D26;

}  // namespace dsp::targets::daisy::hothouse
