#include <dsp/effects/delay.hpp>
#include <dsp/native/offline_renderer.hpp>

#include <cmath>
#include <limits>
#include <vector>

using dsp::effects::DelayProcessor;

int main() {
    DelayProcessor delay;
    if (delay.prepare({0.0, 64, 1})) return 1;

    delay.set_delay_ms(10.0F);
    delay.set_delay_ms(std::numeric_limits<float>::quiet_NaN());
    if (delay.delay_ms() != 10.0F) return 2;
    delay.set_delay_ms(-5.0F);
    if (delay.delay_ms() != 1.0F) return 3;
    delay.set_delay_ms(5000.0F);
    if (delay.delay_ms() != 2000.0F) return 4;

    delay.set_feedback(2.0F);
    if (delay.feedback() != 0.95F) return 5;
    delay.set_feedback(std::numeric_limits<float>::infinity());
    if (delay.feedback() != 0.95F) return 6;

    delay.set_mix(-1.0F);
    if (delay.mix() != 0.0F) return 7;
    delay.set_mix(2.0F);
    if (delay.mix() != 1.0F) return 8;

    delay.set_tempo_bpm(1.0F);
    if (delay.tempo_bpm() != 20.0F) return 9;
    delay.set_tempo_bpm(1000.0F);
    if (delay.tempo_bpm() != 300.0F) return 10;

    delay.set_delay_ms(10.0F);
    delay.set_feedback(0.0F);
    delay.set_mix(1.0F);
    std::vector<float> impulse(1001, 0.0F);
    impulse[0] = 1.0F;
    auto result = dsp::native::render_offline(delay, {48000.0, 64, 1}, {impulse}, 64);
    if (!result) return 11;
    if (std::abs(result.channels[0][480] - 1.0F) > 1.0e-6F) return 12;

    delay.set_feedback(0.5F);
    result = dsp::native::render_offline(delay, {48000.0, 64, 1}, {impulse}, 64);
    if (!result) return 13;
    if (std::abs(result.channels[0][480] - 1.0F) > 1.0e-6F) return 14;
    if (std::abs(result.channels[0][960] - 0.5F) > 1.0e-6F) return 15;

    delay.set_mix(0.0F);
    const std::vector<float> dry{1.0F, -0.5F, 0.25F, 0.0F};
    result = dsp::native::render_offline(delay, {1000.0, 4, 1}, {dry}, 4);
    if (!result || result.channels[0] != dry) return 16;

    DelayProcessor reused;
    reused.set_delay_ms(1.0F);
    reused.set_feedback(0.0F);
    reused.set_mix(1.0F);
    if (!reused.prepare({1000.0, 4, 1})) return 17;
    reused.reset();
    float seeded[]{1.0F, 0.0F};
    float* seeded_channels[]{seeded};
    reused.process({seeded_channels, 1, 2});
    if (!reused.prepare({2000.0, 4, 2})) return 18;
    float left[]{0.0F, 0.0F, 0.0F};
    float right[]{0.0F, 0.0F, 0.0F};
    float* stereo[]{left, right};
    reused.process({stereo, 2, 3});
    for (float value : left) if (value != 0.0F) return 19;
    for (float value : right) if (value != 0.0F) return 20;

    DelayProcessor bounds;
    bounds.set_delay_ms(1.0F);
    bounds.set_feedback(0.0F);
    bounds.set_mix(1.0F);
    if (!bounds.prepare({1000.0, 2, 1})) return 21;
    bounds.reset();
    float oversized[]{1.0F, 2.0F, 3.0F};
    float* oversized_channels[]{oversized};
    bounds.process({oversized_channels, 1, 3});
    if (oversized[0] != 1.0F || oversized[1] != 2.0F || oversized[2] != 3.0F) return 22;

    DelayProcessor null_channel;
    null_channel.set_delay_ms(1.0F);
    null_channel.set_feedback(0.0F);
    null_channel.set_mix(1.0F);
    if (!null_channel.prepare({1000.0, 2, 2})) return 23;
    null_channel.reset();
    float valid[]{1.0F, 0.0F};
    float* channels[]{nullptr, valid};
    null_channel.process({channels, 2, 2});
    if (valid[0] != 0.0F || valid[1] != 1.0F) return 24;

    return 0;
}
