#include <dsp/effects/delay_time.hpp>

#include <cmath>

using dsp::effects::DelaySubdivision;
using dsp::effects::resolve_delay_ms;

int main() {
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::free) != 250.0F) return 1;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::quarter) != 500.0F) return 2;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::dotted_eighth) != 375.0F) return 3;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::eighth) != 250.0F) return 4;
    if (std::abs(resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::eighth_triplet) - 166.66667F) > 1.0e-3F) return 5;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::sixteenth) != 125.0F) return 6;
    if (resolve_delay_ms(-10.0F, 120.0F, DelaySubdivision::free) != 1.0F) return 7;
    if (resolve_delay_ms(5000.0F, 120.0F, DelaySubdivision::free) != 2000.0F) return 8;
    if (resolve_delay_ms(250.0F, 1.0F, DelaySubdivision::quarter) != 2000.0F) return 9;
    if (resolve_delay_ms(250.0F, 1000.0F, DelaySubdivision::quarter) != 200.0F) return 10;
    const auto unknown = static_cast<DelaySubdivision>(999U);
    if (resolve_delay_ms(321.0F, 120.0F, unknown) != 321.0F) return 11;
    return 0;
}