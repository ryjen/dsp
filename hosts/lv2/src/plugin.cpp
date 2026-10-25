#include <lv2/core/lv2.h>
#include <dsp/audio_block.hpp>
#if defined(DSP_LV2_DELAY) && !defined(DSP_LV2_TREMOLO)
#include <dsp/effects/delay.hpp>
#elif defined(DSP_LV2_TREMOLO) && !defined(DSP_LV2_DELAY)
#include <dsp/effects/tremolo.hpp>
#else
#error "Select exactly one LV2 effect"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace {
#if defined(DSP_LV2_DELAY)
using Effect = dsp::effects::DelayProcessor;
constexpr const char* uri = "https://micrantha.com/lv2/delay";
constexpr std::size_t port_count = 9;
#else
using Effect = dsp::effects::TremoloProcessor;
constexpr const char* uri = "https://micrantha.com/lv2/tremolo";
constexpr std::size_t port_count = 7;
#endif
constexpr std::size_t chunk_frames = 2048;
struct Instance {
    Effect effect;
    std::array<float*, port_count> ports{};
};
float value(const Instance& p, std::size_t index, float fallback) noexcept {
    const float* v = p.ports[index];
    return v && std::isfinite(*v) ? *v : fallback;
}
void controls(Instance& p) noexcept {
#if defined(DSP_LV2_DELAY)
    p.effect.set_delay_ms(value(p, 4, 250.0F));
    p.effect.set_feedback(value(p, 5, 0.0F));
    p.effect.set_mix(value(p, 6, 0.5F));
    p.effect.set_tempo_bpm(value(p, 7, 120.0F));
    const float sub = std::clamp(value(p, 8, 0.0F), 0.0F, 5.0F);
    p.effect.set_subdivision(static_cast<dsp::effects::DelaySubdivision>(
        static_cast<unsigned>(std::lround(sub))));
#else
    p.effect.set_rate_hz(value(p, 4, 4.0F));
    p.effect.set_depth(value(p, 5, 0.5F));
    p.effect.set_shape(value(p, 6, 0.0F));
#endif
}
LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*,
                       const LV2_Feature* const* features) {
    if (!std::isfinite(rate) || rate < 1.0 || rate > 192000.0) return nullptr;

    // Required by our Turtle metadata: a compliant host must guarantee that
    // no audio input buffer aliases any audio output buffer.
    constexpr const char* in_place_broken =
        "http://lv2plug.in/ns/lv2core#inPlaceBroken";
    bool supported = false;
    for (const LV2_Feature* const* it = features; it && *it; ++it) {
        if ((*it)->URI && std::strcmp((*it)->URI, in_place_broken) == 0) {
            supported = true;
            break;
        }
    }
    if (!supported) return nullptr;

    try {
        auto p = std::make_unique<Instance>();
        if (!p->effect.prepare({rate, chunk_frames, 2})) return nullptr;
        return p.release();
    } catch (...) {
        return nullptr; // Never unwind into the LV2 C ABI.
    }
}
void connect(LV2_Handle handle, uint32_t port, void* data) {
    auto& p = *static_cast<Instance*>(handle);
    if (port < port_count) p.ports[port] = static_cast<float*>(data);
}
void activate(LV2_Handle handle) {
    auto& p = *static_cast<Instance*>(handle);
    controls(p);
    p.effect.reset();
}
void run(LV2_Handle handle, uint32_t count) {
    auto& p = *static_cast<Instance*>(handle);
    controls(p);
    if (!count || !p.ports[2] || !p.ports[3]) return;
    for (std::size_t pos = 0; pos < count;) {
        const std::size_t frames = std::min(chunk_frames, static_cast<std::size_t>(count) - pos);
        std::array<float*, 2> out{p.ports[2] + pos, p.ports[3] + pos};
        for (std::size_t ch = 0; ch < 2; ++ch) {
            const float* in = p.ports[ch];
            if (!in) std::fill_n(out[ch], frames, 0.0F);
            else if (in + pos != out[ch]) {
                // inPlaceBroken in plugin metadata prohibits any cross-port aliasing.
                std::memmove(out[ch], in + pos, frames * sizeof(float));
            }
        }
        p.effect.process({out.data(), 2, frames});
        pos += frames;
    }
}
void cleanup(LV2_Handle handle) { delete static_cast<Instance*>(handle); }
const void* extension_data(const char*) { return nullptr; }
} // namespace

extern "C" LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    static const LV2_Descriptor descriptor{
        uri, instantiate, connect, activate, run, nullptr, cleanup, extension_data
    };
    return index == 0 ? &descriptor : nullptr;
}
