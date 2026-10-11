#include <dsp/effects/tremolo.hpp>
#include <dsp/juce/processor_audio_source.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace {

std::atomic<bool> tracking{false};
std::atomic<std::size_t> allocations{0};

class ConstantAudioSource final : public juce::AudioSource {
public:
    explicit ConstantAudioSource(float value) noexcept : value_{value} {}

    void prepareToPlay(int samples_per_block, double sample_rate) override {
        prepared_ = samples_per_block > 0 && sample_rate > 0.0;
    }

    void releaseResources() override {
        prepared_ = false;
        released_ = true;
    }

    void getNextAudioBlock(
        const juce::AudioSourceChannelInfo& info) override {
        if (!prepared_ || info.buffer == nullptr) return;
        for (int channel = 0;
             channel < info.buffer->getNumChannels();
             ++channel) {
            for (int sample = 0; sample < info.numSamples; ++sample) {
                info.buffer->setSample(
                    channel,
                    info.startSample + sample,
                    value_);
            }
        }
    }

    [[nodiscard]] bool released() const noexcept { return released_; }

private:
    float value_{};
    bool prepared_{};
    bool released_{};
};

bool all_close(
    const juce::AudioBuffer<float>& buffer,
    float expected,
    float tolerance = 1.0e-6F) {
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            if (std::fabs(buffer.getSample(channel, sample) - expected) >
                tolerance) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

void* operator new(std::size_t size) {
    if (tracking.load(std::memory_order_relaxed)) {
        allocations.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept {
    ::operator delete(memory);
}

int main() {
    ConstantAudioSource input{0.25F};
    dsp::effects::TremoloProcessor tremolo;
    tremolo.set_depth(0.0F);

    dsp::juce_host::ProcessorAudioSource host{input, tremolo, 2U};
    host.prepareToPlay(64, 48000.0);
    if (!host.prepared()) return 1;

    juce::AudioBuffer<float> buffer{2, 64};
    buffer.clear();
    juce::AudioSourceChannelInfo info{&buffer, 0, 64};

    allocations.store(0U, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_release);
    host.getNextAudioBlock(info);
    tracking.store(false, std::memory_order_release);
    if (allocations.load(std::memory_order_relaxed) != 0U) return 2;
    if (!all_close(buffer, 0.25F)) return 3;

    tremolo.set_rate_hz(4.0F);
    tremolo.set_depth(1.0F);
    bool modulated = false;
    for (int block = 0; block < 20; ++block) {
        host.getNextAudioBlock(info);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            const float value = buffer.getSample(0, sample);
            if (value < -1.0e-6F || value > 0.250001F) return 4;
            if (std::fabs(value - 0.25F) > 1.0e-4F) {
                modulated = true;
            }
        }
    }
    if (!modulated) return 5;

    host.releaseResources();
    if (host.prepared() || !input.released()) return 6;

    ConstantAudioSource invalid_input{0.5F};
    dsp::effects::TremoloProcessor invalid_tremolo;
    dsp::juce_host::ProcessorAudioSource invalid_channels{
        invalid_input,
        invalid_tremolo,
        dsp::juce_host::ProcessorAudioSource::max_channels + 1U,
    };
    invalid_channels.prepareToPlay(64, 48000.0);
    if (invalid_channels.prepared()) return 7;

    return 0;
}
