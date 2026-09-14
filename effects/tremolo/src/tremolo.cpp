#include <dsp/effects/tremolo.hpp>

#include "tremolo_faust.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace dsp::effects {
namespace {

class ZoneCaptureUI final : public UI {
public:
    float* rate_hz{nullptr};
    float* depth{nullptr};
    float* shape{nullptr};

    void openTabBox(const char*) override {}
    void openHorizontalBox(const char*) override {}
    void openVerticalBox(const char*) override {}
    void closeBox() override {}
    void addButton(const char*, float*) override {}
    void addCheckButton(const char*, float*) override {}
    void addVerticalSlider(const char* label, float* zone, float, float, float, float) override {
        capture(label, zone);
    }
    void addHorizontalSlider(const char* label, float* zone, float, float, float, float) override {
        capture(label, zone);
    }
    void addNumEntry(const char* label, float* zone, float, float, float, float) override {
        capture(label, zone);
    }
    void addHorizontalBargraph(const char*, float*, float, float) override {}
    void addVerticalBargraph(const char*, float*, float, float) override {}
    void addSoundfile(const char*, const char*, Soundfile**) override {}
    void declare(float*, const char*, const char*) override {}

    [[nodiscard]] bool complete() const noexcept {
        return rate_hz != nullptr && depth != nullptr && shape != nullptr;
    }

private:
    void capture(const char* label, float* zone) noexcept {
        const std::string_view name{label};
        if (name == "rate_hz") rate_hz = zone;
        if (name == "depth") depth = zone;
        if (name == "shape") shape = zone;
    }
};

}  // namespace

class TremoloProcessor::Impl {
public:
    bool prepare(ProcessSpec new_spec, float rate, float depth_value, float shape_value) {
        if (!new_spec.valid()) return false;

        faust.init(static_cast<int>(new_spec.sample_rate));
        ui = {};
        faust.buildUserInterface(&ui);
        if (!ui.complete()) return false;

        gain_buffer.assign(new_spec.max_block_size, 1.0F);
        spec = new_spec;
        prepared = true;
        apply_targets(rate, depth_value, shape_value);
        return true;
    }

    void reset(float rate, float depth_value, float shape_value) noexcept {
        if (!prepared) return;
        faust.instanceClear();
        apply_targets(rate, depth_value, shape_value);
    }

    void apply_targets(float rate, float depth_value, float shape_value) noexcept {
        *ui.rate_hz = rate;
        *ui.depth = depth_value;
        *ui.shape = shape_value;
    }

    TremoloFaust faust;
    ZoneCaptureUI ui;
    std::vector<float> gain_buffer;
    ProcessSpec spec{};
    bool prepared{false};
};

TremoloProcessor::TremoloProcessor() : impl_(std::make_unique<Impl>()) {}
TremoloProcessor::~TremoloProcessor() = default;

bool TremoloProcessor::prepare(const ProcessSpec& spec) {
    return impl_->prepare(spec, rate_hz(), depth(), shape());
}

void TremoloProcessor::reset() noexcept {
    impl_->reset(rate_hz(), depth(), shape());
}

void TremoloProcessor::process(AudioBlock block) noexcept {
    if (!impl_->prepared) return;
    if (block.frame_count > impl_->spec.max_block_size) return;
    if (block.channel_count > impl_->spec.channel_count) return;
    if (block.frame_count == 0 || block.channel_count == 0) return;

    impl_->apply_targets(rate_hz(), depth(), shape());
    float* outputs[] = {impl_->gain_buffer.data()};
    impl_->faust.compute(static_cast<int>(block.frame_count), nullptr, outputs);

    for (std::size_t channel = 0; channel < block.channel_count; ++channel) {
        float* samples = block.channel(channel);
        if (samples == nullptr) continue;
        for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
            samples[frame] *= impl_->gain_buffer[frame];
        }
    }
}

void TremoloProcessor::set_rate_hz(float value) noexcept {
    rate_hz_.store(std::clamp(value, 0.1F, 20.0F));
}

void TremoloProcessor::set_depth(float value) noexcept {
    depth_.store(std::clamp(value, 0.0F, 1.0F));
}

void TremoloProcessor::set_shape(float value) noexcept {
    shape_.store(std::clamp(value, 0.0F, 1.0F));
}

float TremoloProcessor::rate_hz() const noexcept { return rate_hz_.load(); }
float TremoloProcessor::depth() const noexcept { return depth_.load(); }
float TremoloProcessor::shape() const noexcept { return shape_.load(); }

}  // namespace dsp::effects
