#include <dsp/effects/tremolo.hpp>
#include <dsp/native/offline_renderer.hpp>

#include <vector>

int main() {
    dsp::effects::TremoloProcessor tremolo;

    tremolo.set_rate_hz(-4.0F);
    tremolo.set_depth(2.0F);
    tremolo.set_shape(-1.0F);
    if (tremolo.rate_hz() != 0.1F) return 1;
    if (tremolo.depth() != 1.0F) return 2;
    if (tremolo.shape() != 0.0F) return 3;

    tremolo.set_rate_hz(25.0F);
    if (tremolo.rate_hz() != 20.0F) return 4;

    tremolo.set_depth(0.0F);
    const std::vector<std::vector<float>> input{
        {0.25F, -0.5F, 0.75F, -1.0F, 0.125F, -0.25F},
        {-0.125F, 0.25F, -0.75F, 1.0F, -0.5F, 0.5F},
    };
    const auto result = dsp::native::render_offline(tremolo, {48000.0, 64, 2}, input, 4);
    if (!result) return 5;
    if (result.channels != input) return 6;

    return 0;
}
