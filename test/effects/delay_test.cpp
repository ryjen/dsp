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

    DelayProcessor fractional;
    fractional.set_delay_ms(1.5F);
    fractional.set_feedback(0.0F);
    fractional.set_mix(1.0F);
    std::vector<float> fractional_impulse(8, 0.0F);
    fractional_impulse[0] = 1.0F;
    result = dsp::native::render_offline(
        fractional, {1000.0, 8, 1}, {fractional_impulse}, 8);
    if (!result) return 25;
    if (std::abs(result.channels[0][1] - 0.5F) > 1.0e-6F) return 26;
    if (std::abs(result.channels[0][2] - 0.5F) > 1.0e-6F) return 27;

    DelayProcessor synced;
    synced.set_tempo_bpm(120.0F);
    synced.set_subdivision(dsp::effects::DelaySubdivision::quarter);
    synced.set_feedback(0.0F);
    synced.set_mix(1.0F);
    std::vector<float> sync_impulse(600, 0.0F);
    sync_impulse[0] = 1.0F;
    result = dsp::native::render_offline(
        synced, {1000.0, 64, 1}, {sync_impulse}, 64);
    if (!result) return 28;
    if (std::abs(result.channels[0][500] - 1.0F) > 1.0e-6F) return 29;

    DelayProcessor time_transition;
    time_transition.set_delay_ms(10.0F);
    time_transition.set_feedback(0.0F);
    time_transition.set_mix(1.0F);
    if (!time_transition.prepare({1000.0, 600, 1})) return 30;
    time_transition.reset();
    std::vector<float> time_lead(600);
    for (std::size_t i = 0; i < time_lead.size(); ++i) {
        time_lead[i] = static_cast<float>(i) * 0.01F;
    }
    float* time_lead_channels[]{time_lead.data()};
    time_transition.process({time_lead_channels, 1, time_lead.size()});
    float previous = time_lead.back();
    time_transition.set_delay_ms(500.0F);
    std::vector<float> time_changed(128);
    for (std::size_t i = 0; i < time_changed.size(); ++i) {
        time_changed[i] = static_cast<float>(600 + i) * 0.01F;
    }
    float* time_changed_channels[]{time_changed.data()};
    time_transition.process({time_changed_channels, 1, time_changed.size()});
    for (float value : time_changed) {
        if (std::abs(value - previous) > 0.25F) return 31;
        previous = value;
    }

    DelayProcessor tempo_transition;
    tempo_transition.set_tempo_bpm(120.0F);
    tempo_transition.set_subdivision(dsp::effects::DelaySubdivision::quarter);
    tempo_transition.set_feedback(0.0F);
    tempo_transition.set_mix(1.0F);
    if (!tempo_transition.prepare({1000.0, 1200, 1})) return 32;
    tempo_transition.reset();
    std::vector<float> tempo_lead(1200);
    for (std::size_t i = 0; i < tempo_lead.size(); ++i) {
        tempo_lead[i] = static_cast<float>(i) * 0.01F;
    }
    float* tempo_lead_channels[]{tempo_lead.data()};
    tempo_transition.process({tempo_lead_channels, 1, tempo_lead.size()});
    previous = tempo_lead.back();
    tempo_transition.set_tempo_bpm(60.0F);
    std::vector<float> tempo_changed(128);
    for (std::size_t i = 0; i < tempo_changed.size(); ++i) {
        tempo_changed[i] = static_cast<float>(1200 + i) * 0.01F;
    }
    float* tempo_changed_channels[]{tempo_changed.data()};
    tempo_transition.process({tempo_changed_channels, 1, tempo_changed.size()});
    for (float value : tempo_changed) {
        if (std::abs(value - previous) > 0.25F) return 33;
        previous = value;
    }

    DelayProcessor mix_transition;
    mix_transition.set_delay_ms(100.0F);
    mix_transition.set_feedback(0.0F);
    mix_transition.set_mix(0.0F);
    if (!mix_transition.prepare({1000.0, 600, 1})) return 34;
    mix_transition.reset();
    std::vector<float> mix_lead(600);
    for (std::size_t i = 0; i < mix_lead.size(); ++i) {
        mix_lead[i] = static_cast<float>(i) * 0.01F;
    }
    float* mix_lead_channels[]{mix_lead.data()};
    mix_transition.process({mix_lead_channels, 1, mix_lead.size()});
    previous = mix_lead.back();
    mix_transition.set_mix(1.0F);
    std::vector<float> mix_changed(64);
    for (std::size_t i = 0; i < mix_changed.size(); ++i) {
        mix_changed[i] = static_cast<float>(600 + i) * 0.01F;
    }
    float* mix_changed_channels[]{mix_changed.data()};
    mix_transition.process({mix_changed_channels, 1, mix_changed.size()});
    for (float value : mix_changed) {
        if (std::abs(value - previous) > 0.25F) return 35;
        previous = value;
    }

    DelayProcessor feedback_transition;
    feedback_transition.set_delay_ms(1.0F);
    feedback_transition.set_feedback(0.0F);
    feedback_transition.set_mix(1.0F);
    if (!feedback_transition.prepare({1000.0, 4, 1})) return 36;
    feedback_transition.reset();
    float feedback_seed[]{1.0F};
    float* feedback_seed_channels[]{feedback_seed};
    feedback_transition.process({feedback_seed_channels, 1, 1});
    feedback_transition.set_feedback(0.95F);
    float feedback_changed[]{0.0F, 0.0F};
    float* feedback_changed_channels[]{feedback_changed};
    feedback_transition.process({feedback_changed_channels, 1, 2});
    if (std::abs(feedback_changed[0] - 1.0F) > 1.0e-6F) return 37;
    if (!(feedback_changed[1] > 0.0F && feedback_changed[1] < 0.2F)) return 38;

    DelayProcessor stable;
    stable.set_delay_ms(10.0F);
    stable.set_feedback(0.95F);
    stable.set_mix(1.0F);
    const std::vector<float> dc_input(4000, 1.0F);
    result = dsp::native::render_offline(
        stable, {1000.0, 64, 1}, {dc_input}, 64);
    if (!result) return 39;
    for (float value : result.channels[0]) {
        if (!std::isfinite(value)) return 40;
        if (std::abs(value) > 20.001F) return 41;
    }

    DelayProcessor silent;
    silent.set_delay_ms(10.0F);
    silent.set_feedback(0.95F);
    silent.set_mix(1.0F);
    const std::vector<float> silence(1024, 0.0F);
    result = dsp::native::render_offline(
        silent, {1000.0, 64, 1}, {silence}, 64);
    if (!result) return 39;
    for (float value : result.channels[0]) if (value != 0.0F) return 43;

    return 0;
}