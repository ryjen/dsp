#pragma once

#include <dsp/processor.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstddef>

namespace dsp::juce_host {

class ProcessorAudioSource final : public juce::AudioSource {
public:
    static constexpr std::size_t max_channels = 8U;

    ProcessorAudioSource(
        juce::AudioSource& input,
        dsp::Processor& processor,
        std::size_t channel_count) noexcept;

    void prepareToPlay(
        int samples_per_block_expected,
        double sample_rate) override;
    void releaseResources() override;
    void getNextAudioBlock(
        const juce::AudioSourceChannelInfo& buffer_to_fill) override;

    [[nodiscard]] bool prepared() const noexcept { return prepared_; }

private:
    juce::AudioSource& input_;
    dsp::Processor& processor_;
    std::size_t channel_count_{};
    std::size_t max_block_size_{};
    bool prepared_{};
};

}  // namespace dsp::juce_host
