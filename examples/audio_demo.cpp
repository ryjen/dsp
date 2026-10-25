// Synthetic plucked-string demo using the real portable DSP processors.
// Generates reproducible stereo PCM WAV files, no third-party assets.
#include <dsp/effects/delay.hpp>
#include <dsp/effects/tremolo.hpp>
#include <dsp/native/offline_renderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

namespace {
constexpr int rate = 48000;
constexpr std::size_t frames = static_cast<std::size_t>(rate) * 4;
using Audio = std::vector<std::vector<float>>;

Audio plucked_fixture() {
    Audio result(2, std::vector<float>(frames, 0.0F));
    constexpr std::array<double, 6> tones{110.0, 146.8324, 164.8138, 196.0, 220.0, 246.9417};
    for (std::size_t i = 0; i < tones.size(); ++i) {
        const std::size_t start = static_cast<std::size_t>((0.12 + i * 0.50) * rate);
        for (std::size_t n = 0; n < static_cast<std::size_t>(rate) && start + n < frames; ++n) {
            const double t = static_cast<double>(n) / rate;
            const double phase = 2.0 * std::numbers::pi_v<double> * tones[i] * t;
            const double decay = std::exp(-4.2 * t);
            const double attack = std::min(1.0, t * 1600.0);
            const float sample = static_cast<float>(
                decay * attack * (0.22 * std::sin(phase) +
                0.095 * std::sin(2.0 * phase) + 0.055 * std::sin(3.0 * phase)));
            result[0][start + n] += sample;
            result[1][start + n] += sample * 0.93F;
        }
    }
    return result;
}

void put16(std::ofstream& f, std::uint16_t x) {
    f.put(static_cast<char>(x & 255U));
    f.put(static_cast<char>((x >> 8U) & 255U));
}
void put32(std::ofstream& f, std::uint32_t x) {
    put16(f, static_cast<std::uint16_t>(x));
    put16(f, static_cast<std::uint16_t>(x >> 16U));
}
bool wav(const std::filesystem::path& path, const Audio& data) {
    if (data.size() != 2 || data[0].size() != frames || data[1].size() != frames)
        return false;
    const auto byte_count = static_cast<std::uint32_t>(frames * 2 * sizeof(std::int16_t));
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write("RIFF", 4); put32(out, 36U + byte_count);
    out.write("WAVEfmt ", 8); put32(out, 16); put16(out, 1);
    put16(out, 2); put32(out, rate); put32(out, rate * 4);
    put16(out, 4); put16(out, 16);
    out.write("data", 4); put32(out, byte_count);
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t c = 0; c < 2; ++c) {
            if (!std::isfinite(data[c][i])) return false;
            const float safe = std::clamp(data[c][i], -1.0F, 1.0F);
            const auto pcm = static_cast<std::int16_t>(
                std::lround(safe * 32767.0F));
            put16(out, static_cast<std::uint16_t>(pcm));
        }
    }
    return out.good();
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: dsp_effects_audio_demo OUTPUT_DIRECTORY\n";
        return 2;
    }
    const dsp::ProcessSpec spec{static_cast<double>(rate), 128, 2};
    const Audio dry = plucked_fixture();

    dsp::effects::TremoloProcessor tremolo;
    tremolo.set_rate_hz(4.3F);
    tremolo.set_depth(0.78F);
    tremolo.set_shape(0.25F);
    auto t = dsp::native::render_offline(tremolo, spec, dry, 128);
    if (!t) return 3;

    dsp::effects::DelayProcessor delay;
    delay.set_delay_ms(330.0F);
    delay.set_feedback(0.42F);
    delay.set_mix(0.43F);
    auto wet = dsp::native::render_offline(delay, spec, t.channels, 128);
    if (!wet) return 4;
    bool changed = false;
    for (std::size_t i = 0; i < frames; ++i) {
        if (std::abs(wet.channels[0][i] - dry[0][i]) > 0.001F) {
            changed = true;
            break;
        }
    }
    if (!changed) return 5;

    try {
        std::filesystem::create_directories(argv[1]);
        const std::filesystem::path output{argv[1]};
        if (!wav(output / "dry.wav", dry)) return 6;
        if (!wav(output / "tremolo.wav", t.channels)) return 7;
        if (!wav(output / "tremolo-delay.wav", wet.channels)) return 8;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 9;
    }
    std::cout << "Created dry.wav, tremolo.wav, tremolo-delay.wav "
              << "(stereo 48kHz 16-bit, synthetic plucked-string fixture)\n";
    return 0;
}
