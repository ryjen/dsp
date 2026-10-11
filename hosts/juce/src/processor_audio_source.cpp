#include <dsp/juce/processor_audio_source.hpp>

namespace dsp::juce_host {

ProcessorAudioSource::ProcessorAudioSource(
    juce::AudioSource& input,
    dsp::Processor& processor,
    std::size_t channel_count) noexcept
    : input_{input},
      processor_{processor},
      channel_count_{channel_count} {}

void ProcessorAudioSource::prepareToPlay(
    int samples_per_block_expected,
    double sample_rate) {
    input_.prepareToPlay(samples_per_block_expected, sample_rate);
    prepared_ = false;
    max_block_size_ = 0U;

    if (samples_per_block_expected <= 0 ||
        sample_rate <= 0.0 ||
        channel_count_ == 0U ||
        channel_count_ > max_channels) {
        return;
    }

    max_block_size_ =
        static_cast<std::size_t>(samples_per_block_expected);
    prepared_ = processor_.prepare(
        {
            sample_rate,
            max_block_size_,
            channel_count_,
        });
    if (prepared_) processor_.reset();
}

void ProcessorAudioSource::releaseResources() {
    processor_.reset();
    prepared_ = false;
    max_block_size_ = 0U;
    input_.releaseResources();
}

void ProcessorAudioSource::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& buffer_to_fill) {
    input_.getNextAudioBlock(buffer_to_fill);

    if (!prepared_ ||
        buffer_to_fill.buffer == nullptr ||
        buffer_to_fill.startSample < 0 ||
        buffer_to_fill.numSamples <= 0 ||
        static_cast<std::size_t>(buffer_to_fill.numSamples) >
            max_block_size_ ||
        buffer_to_fill.buffer->getNumChannels() <
            static_cast<int>(channel_count_)) {
        return;
    }

    std::array<float*, max_channels> channels{};
    for (std::size_t channel = 0U;
         channel < channel_count_;
         ++channel) {
        channels[channel] = buffer_to_fill.buffer->getWritePointer(
            static_cast<int>(channel),
            buffer_to_fill.startSample);
    }

    processor_.process(
        {
            channels.data(),
            channel_count_,
            static_cast<std::size_t>(buffer_to_fill.numSamples),
        });
}

}  // namespace dsp::juce_host
