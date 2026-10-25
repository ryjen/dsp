// Bounded offline WAV renderer. CLI process I/O remains outside realtime DSP.
#include <dsp/effects/delay.hpp>
#include <dsp/effects/tremolo.hpp>
#include <dsp/native/offline_renderer.hpp>
#include <dsp/protocols/delay_preset_binding.hpp>
#include <dsp/protocols/guitar_dsp_preset_v1.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Audio = std::vector<std::vector<float>>;
constexpr std::size_t max_pcm_bytes = 64U * 1024U * 1024U;
constexpr std::size_t block_frames = 128;

struct Wav {
    Audio channels;
    std::uint32_t rate{};
};

[[noreturn]] void invalid(std::string_view reason) {
    throw std::runtime_error(std::string(reason));
}
std::uint16_t u16(std::istream& in) {
    unsigned char bytes[2]{};
    if (!in.read(reinterpret_cast<char*>(bytes), 2)) invalid("truncated WAV");
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8U));
}
std::uint32_t u32(std::istream& in) {
    return static_cast<std::uint32_t>(u16(in)) |
        (static_cast<std::uint32_t>(u16(in)) << 16U);
}
std::string tag(std::istream& in) {
    std::array<char, 4> bytes{};
    if (!in.read(bytes.data(), 4)) invalid("truncated WAV header");
    return {bytes.data(), bytes.size()};
}
Wav read_wav(const std::filesystem::path& path) {
    std::error_code ec;
    const auto file_bytes = std::filesystem::file_size(path, ec);
    if (ec || file_bytes < 44U || file_bytes > max_pcm_bytes + 65536U)
        invalid("WAV must be an existing, bounded RIFF file (maximum 64 MiB PCM)");
    std::ifstream in(path, std::ios::binary);
    if (!in || tag(in) != "RIFF") invalid("expected RIFF WAV");
    const std::uint32_t riff_bytes = u32(in);
    if (tag(in) != "WAVE" || static_cast<std::uint64_t>(riff_bytes) + 8U != file_bytes)
        invalid("invalid RIFF size or WAVE header");
    std::uint16_t channels = 0, bits = 0, codec = 0, align = 0;
    std::uint32_t rate = 0, bytes_per_second = 0;
    std::uint64_t data_pos = 0;
    std::uint32_t data_bytes = 0;
    bool have_fmt = false, have_data = false;
    std::uint64_t pos = 12;
    while (pos + 8U <= file_bytes) {
        in.seekg(static_cast<std::streamoff>(pos));
        const std::string id = tag(in);
        const std::uint32_t length = u32(in);
        const std::uint64_t start = pos + 8U;
        const std::uint64_t end = start + length;
        if (end > file_bytes || end + (length & 1U) > file_bytes)
            invalid("invalid WAV chunk bounds");
        if (id == "fmt ") {
            if (have_fmt || length < 16U) invalid("duplicate or short fmt chunk");
            codec = u16(in); channels = u16(in); rate = u32(in);
            bytes_per_second = u32(in); align = u16(in); bits = u16(in);
            have_fmt = true;
        } else if (id == "data") {
            if (have_data) invalid("multiple WAV data chunks unsupported");
            data_pos = start; data_bytes = length; have_data = true;
        }
        pos = end + (length & 1U);
    }
    if (!have_fmt || !have_data || codec != 1U || bits != 16U ||
        (channels != 1U && channels != 2U) ||
        rate < 8000U || rate > 192000U || align != channels * 2U ||
        bytes_per_second != rate * align || data_bytes == 0U ||
        data_bytes > max_pcm_bytes || data_bytes % align != 0U)
        invalid("only bounded mono/stereo PCM16 WAV (8-192 kHz) is supported");
    const std::size_t frames = data_bytes / align;
    Wav result{Audio(channels, std::vector<float>(frames)), rate};
    in.seekg(static_cast<std::streamoff>(data_pos));
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const auto value = static_cast<std::int16_t>(u16(in));
            result.channels[channel][frame] = static_cast<float>(value) / 32768.0F;
        }
    }
    return result;
}
void write_u16(std::FILE* out, std::uint16_t value) {
    if (std::fputc(value & 255U, out) == EOF ||
        std::fputc((value >> 8U) & 255U, out) == EOF) invalid("WAV output failed");
}
void write_u32(std::FILE* out, std::uint32_t value) {
    write_u16(out, static_cast<std::uint16_t>(value));
    write_u16(out, static_cast<std::uint16_t>(value >> 16U));
}
void write_tag(std::FILE* out, const char* value) {
    if (std::fwrite(value, 1, 4, out) != 4U) invalid("WAV output failed");
}
void write_wav(const std::filesystem::path& path, const Wav& wav) {
    const std::size_t channel_count = wav.channels.size();
    if ((channel_count != 1 && channel_count != 2) || wav.channels[0].empty())
        invalid("empty or invalid render");
    const std::size_t frames = wav.channels[0].size();
    for (const auto& channel : wav.channels)
        if (channel.size() != frames) invalid("different output channel lengths");
    const auto data_bytes = frames * channel_count * 2U;
    if (data_bytes > max_pcm_bytes || data_bytes > UINT32_MAX - 36U)
        invalid("output exceeds WAV PCM16 size bound");
    // The C11 'x' flag rejects an existing file or symlink. Never overwrite.
    std::FILE* out = std::fopen(path.string().c_str(), "wbx");
    if (!out) invalid("cannot create output; directory missing or file exists");
    bool complete = false;
    try {
        write_tag(out, "RIFF"); write_u32(out, static_cast<std::uint32_t>(36U + data_bytes));
        if (std::fwrite("WAVEfmt ", 1, 8, out) != 8U) invalid("WAV output failed");
        write_u32(out, 16U); write_u16(out, 1U);
        write_u16(out, static_cast<std::uint16_t>(channel_count));
        write_u32(out, wav.rate);
        write_u32(out, wav.rate * static_cast<std::uint32_t>(channel_count) * 2U);
        write_u16(out, static_cast<std::uint16_t>(channel_count * 2U));
        write_u16(out, 16U);
        write_tag(out, "data"); write_u32(out, static_cast<std::uint32_t>(data_bytes));
        for (std::size_t i = 0; i < frames; ++i) {
            for (std::size_t channel = 0; channel < channel_count; ++channel) {
                const float value = wav.channels[channel][i];
                if (!std::isfinite(value)) invalid("DSP produced non-finite audio");
                const float clipped = std::clamp(value, -1.0F, 1.0F);
                const auto sample = static_cast<std::int16_t>(
                    std::clamp(std::lround(clipped * 32768.0F), -32768L, 32767L));
                write_u16(out, static_cast<std::uint16_t>(sample));
            }
        }
        complete = std::fclose(out) == 0;
    } catch (...) {
        std::fclose(out);
        std::filesystem::remove(path);
        throw;
    }
    if (!complete) {
        std::filesystem::remove(path);
        invalid("unable to flush WAV output");
    }
}
float number(std::string_view value, float lo, float hi) {
    const std::string text{value};
    char* tail = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &tail);
    if (errno == ERANGE || tail == text.c_str() || *tail != '\0' ||
        !std::isfinite(parsed) || parsed < lo || parsed > hi)
        invalid("invalid numeric effect parameter");
    return parsed;
}
struct Options {
    std::string input, output, preset, effects{"delay"};
    float tremolo_rate{4.3F}, tremolo_depth{0.78F}, tremolo_shape{0.25F};
    float delay_ms{330.0F}, feedback{0.42F}, mix{0.43F}, tempo{120.0F};
    std::string subdivision{"free"};
    bool delay_override{false};
    bool tremolo_override{false};
};
const char* usage =
    "Usage: dsp render INPUT.wav --output OUTPUT.wav [--preset CANONICAL-V1.json]\n"
    "  [--effects none|delay|tremolo|tremolo,delay|delay,tremolo]\n"
    "  [--delay-ms 1..2000 --feedback 0..0.95 --mix 0..1 --tempo 20..300]\n"
    "  [--subdivision free|quarter|dotted-eighth|eighth|eighth-triplet|sixteenth]\n"
    "  [--tremolo-rate 0.1..20 --tremolo-depth 0..1 --tremolo-shape 0..1]\n"
    "  Only PCM16 mono/stereo RIFF WAV, no overwrite, no realtime audio.\n";
Options arguments(int argc, char** argv) {
    if (argc < 2 || std::strcmp(argv[1], "render") != 0) invalid(usage);
    Options opt;
    bool got_input = false;
    for (int i = 2; i < argc; ++i) {
        const std::string key{argv[i]};
        if (key == "--help") { std::cout << usage; std::exit(0); }
        if (!key.empty() && key[0] != '-') {
            if (got_input) invalid("only one input WAV is supported");
            opt.input = key;
            got_input = true;
            continue;
        }
        if (i + 1 >= argc) invalid("missing option value");
        const std::string val{argv[++i]};
        if (key == "--output") opt.output = val;
        else if (key == "--preset") opt.preset = val;
        else if (key == "--effects") opt.effects = val;
        else if (key == "--delay-ms") {opt.delay_ms = number(val, 1.0F, 2000.0F);opt.delay_override = true;}
        else if (key == "--feedback") {opt.feedback = number(val, 0.0F, 0.95F);opt.delay_override = true;}
        else if (key == "--mix") {opt.mix = number(val, 0.0F, 1.0F);opt.delay_override = true;}
        else if (key == "--tempo") {opt.tempo = number(val, 20.0F, 300.0F);opt.delay_override = true;}
        else if (key == "--subdivision") {opt.subdivision = val;opt.delay_override = true;}
        else if (key == "--tremolo-rate") {opt.tremolo_rate = number(val, 0.1F, 20.0F);opt.tremolo_override = true;}
        else if (key == "--tremolo-depth") {opt.tremolo_depth = number(val, 0.0F, 1.0F);opt.tremolo_override = true;}
        else if (key == "--tremolo-shape") {opt.tremolo_shape = number(val, 0.0F, 1.0F);opt.tremolo_override = true;}
        else invalid("unknown dsp option");
    }
    const bool delay = opt.effects == "delay" || opt.effects == "tremolo,delay" ||
        opt.effects == "delay,tremolo";
    const bool tremolo = opt.effects == "tremolo" || opt.effects == "tremolo,delay" ||
        opt.effects == "delay,tremolo";
    if (!got_input || opt.output.empty() ||
        (!delay && !tremolo && opt.effects != "none"))
        invalid(usage);
    if ((!delay && (!opt.preset.empty() || opt.delay_override)) ||
        (!tremolo && opt.tremolo_override) ||
        (!opt.preset.empty() && opt.delay_override))
        invalid("effect options conflict with selected chain or preset");
    const std::array<std::string_view, 6> subdivisions{
        "free", "quarter", "dotted-eighth", "eighth", "eighth-triplet", "sixteenth"
    };
    if (std::find(subdivisions.begin(), subdivisions.end(), opt.subdivision) ==
        subdivisions.end()) invalid("unknown delay subdivision");
    return opt;
}
void apply(dsp::effects::DelayProcessor& delay, const Options& opt) {
    if (!opt.preset.empty()) {
        std::ifstream input(opt.preset, std::ios::binary);
        if (!input) invalid("cannot read preset");
        std::array<char, 257> buffer{};
        input.read(buffer.data(), buffer.size());
        if (input.gcount() <= 0 || input.gcount() > 256) invalid("oversized or empty preset");
        const auto parsed = dsp::protocols::parse_preset_v1(
            {buffer.data(), static_cast<std::size_t>(input.gcount())});
        if (!parsed) invalid("preset must be canonical guitar-dsp.preset v1");
        if (dsp::protocols::apply_delay_preset_v1(parsed.value, delay) !=
            dsp::protocols::ProtocolError::none) invalid("invalid delay preset");
        return;
    }
    delay.set_delay_ms(opt.delay_ms);
    delay.set_feedback(opt.feedback);
    delay.set_mix(opt.mix);
    delay.set_tempo_bpm(opt.tempo);
    const std::array<std::string_view, 6> values{
        "free","quarter","dotted-eighth","eighth","eighth-triplet","sixteenth"
    };
    const auto index = std::find(values.begin(), values.end(), opt.subdivision) - values.begin();
    delay.set_subdivision(static_cast<dsp::effects::DelaySubdivision>(index));
}
void process(Wav& wav, const Options& opt) {
    const dsp::ProcessSpec spec{static_cast<double>(wav.rate),
        block_frames, wav.channels.size()};
    auto render = [&](dsp::Processor& processor) {
        auto result = dsp::native::render_offline(
            processor, spec, wav.channels, block_frames);
        if (!result) invalid("effect preparation or processing failed");
        wav.channels = std::move(result.channels);
    };
    const auto tremolo = [&] {
        dsp::effects::TremoloProcessor processor;
        processor.set_rate_hz(opt.tremolo_rate);
        processor.set_depth(opt.tremolo_depth);
        processor.set_shape(opt.tremolo_shape);
        render(processor);
    };
    const auto delay = [&] {
        dsp::effects::DelayProcessor processor;
        apply(processor, opt);
        render(processor);
    };
    if (opt.effects == "tremolo,delay") {tremolo(); delay();}
    else if (opt.effects == "delay,tremolo") {delay(); tremolo();}
    else if (opt.effects == "delay") delay();
    else if (opt.effects == "tremolo") tremolo();
}
}

int main(int argc, char** argv) {
    try {
        const auto opt = arguments(argc, argv);
        Wav wav = read_wav(opt.input);
        process(wav, opt);
        write_wav(opt.output, wav);
        std::cout << "Wrote " << opt.output << " (" << wav.rate << " Hz, "
                  << wav.channels.size() << " channel PCM16 WAV)\n";
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << "dsp: " << exc.what() << '\n';
        return 1;
    }
}
