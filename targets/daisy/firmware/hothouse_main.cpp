#include "daisy.h"
#include "daisy_monotonic_clock.hpp"
#include "hothouse_config.hpp"
#include "hothouse_hardware.hpp"
#include "libdaisy_midi_bridge.hpp"

#include <dsp/control/pedal_control_state.hpp>
#include <dsp/effects/delay.hpp>
#include <dsp/process_spec.hpp>
#include <dsp/targets/daisy/control_mapper.hpp>
#include <dsp/targets/daisy/delay_control_binding.hpp>
#include <dsp/targets/daisy/midi_adapter.hpp>
#include <dsp/targets/daisy/pedal_runtime.hpp>
#include <dsp/targets/daisy/tempo_controller.hpp>

#include <cstddef>
#include <span>

namespace {

using dsp::control::PedalControlState;
using dsp::effects::DelayProcessor;
using dsp::targets::daisy::ControlEventKind;
using dsp::targets::daisy::ControlMapper;
using dsp::targets::daisy::DelayControlBinding;
using dsp::targets::daisy::MidiAdapter;
using dsp::targets::daisy::PedalRuntime;
using dsp::targets::daisy::TempoController;
using dsp::targets::daisy::firmware::DaisyMonotonicClock;
using dsp::targets::daisy::firmware::HothouseHardware;
using dsp::targets::daisy::firmware::LibDaisyMidiBridge;
namespace target = dsp::targets::daisy::firmware;

float DSY_SDRAM_BSS delay_storage[target::delay_storage_samples];

class HothouseApp {
public:
    HothouseApp()
        : hardware_{seed_},
          mapper_{control_state_, tempo_},
          delay_{std::span<float>{delay_storage}} {}

    bool init() {
        seed_.Init(true);
        seed_.SetAudioBlockSize(target::block_size);
        seed_.SetAudioSampleRate(
            ::daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);

        hardware_.init();

        ::daisy::MidiUsbHandler::Config midi_config;
        midi_config.transport_config.periph =
            ::daisy::MidiUsbTransport::Config::INTERNAL;
        midi_.Init(midi_config);

        const dsp::ProcessSpec spec{
            target::sample_rate_hz,
            target::block_size,
            target::channel_count,
        };
        const auto required = DelayProcessor::required_storage_samples(spec);
        if (!required || *required > target::delay_storage_samples) {
            return false;
        }
        if (!runtime_.prepare(spec, delay_)) return false;

        load_meter_.Init(
            static_cast<float>(target::sample_rate_hz),
            static_cast<int>(target::block_size));

        hardware_.start_adc();
        midi_.StartReceive();
        return true;
    }

    void start_audio(::daisy::AudioHandle::AudioCallback callback) {
        seed_.StartAudio(callback);
    }

    void process_audio(
        ::daisy::AudioHandle::InputBuffer input,
        ::daisy::AudioHandle::OutputBuffer output,
        std::size_t frames) noexcept {
        load_meter_.OnBlockStart();
        runtime_.process(input, output, target::channel_count, frames);
        load_meter_.OnBlockEnd();
    }

    void run_control_iteration() noexcept {
        const auto now = clock_.now();

        mapper_.update_pot1(hardware_.pot1());
        mapper_.update_pot2(hardware_.pot2());
        mapper_.update_toggle1(hardware_.toggle1_position(), now);
        mapper_.update_footswitch1(hardware_.footswitch1_pressed(), now);
        mapper_.update_footswitch2(hardware_.footswitch2_pressed(), now);

        midi_.Listen();
        while (midi_.HasEvents()) {
            const auto message =
                LibDaisyMidiBridge::translate(midi_.PopEvent());
            const auto control = midi_adapter_.translate(message, now);
            if (control) handle_control(*control);
        }

        tempo_.update_timeout(now);

        auto snapshot = control_state_.snapshot();
        snapshot.tempo_bpm = tempo_.tempo_bpm();
        snapshot.transport_running = transport_running_;
        control_state_.publish(snapshot);

        snapshot = control_state_.snapshot();
        DelayControlBinding::apply(delay_, snapshot);
        runtime_.set_bypassed(snapshot.bypassed);
    }

private:
    void handle_control(
        const dsp::targets::daisy::ControlEvent& event) noexcept {
        switch (event.kind) {
            case ControlEventKind::midi_clock:
                tempo_.on_midi_clock(event.timestamp);
                break;
            case ControlEventKind::transport_start:
                transport_running_ = true;
                tempo_.on_midi_start(event.timestamp);
                break;
            case ControlEventKind::transport_stop:
                transport_running_ = false;
                tempo_.on_midi_stop();
                break;
            case ControlEventKind::transport_continue:
                transport_running_ = true;
                tempo_.on_midi_continue(event.timestamp);
                break;
            case ControlEventKind::primary_normalized:
                mapper_.apply_midi_control(20U, event.normalized);
                break;
            case ControlEventKind::secondary_normalized:
                mapper_.apply_midi_control(21U, event.normalized);
                break;
        }
    }

    ::daisy::DaisySeed seed_;
    HothouseHardware hardware_;
    ::daisy::MidiUsbHandler midi_;
    ::daisy::CpuLoadMeter load_meter_;
    DaisyMonotonicClock clock_;
    PedalControlState control_state_;
    TempoController tempo_;
    MidiAdapter midi_adapter_;
    ControlMapper mapper_;
    DelayProcessor delay_;
    PedalRuntime runtime_;
    bool transport_running_{false};
};

HothouseApp* active_app = nullptr;

void audio_callback(
    ::daisy::AudioHandle::InputBuffer input,
    ::daisy::AudioHandle::OutputBuffer output,
    std::size_t frames) {
    if (active_app != nullptr) {
        active_app->process_audio(input, output, frames);
    }
}

}  // namespace

int main() {
    HothouseApp app;
    active_app = &app;

    if (!app.init()) {
        for (;;) {
        }
    }

    app.start_audio(audio_callback);
    for (;;) {
        app.run_control_iteration();
        ::daisy::System::Delay(1);
    }
}
