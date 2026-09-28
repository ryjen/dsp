#include <dsp/targets/daisy/tempo_controller.hpp>

#include <cmath>
#include <cstdint>

namespace {
bool near(float actual, float expected, float tolerance = 0.1F) {
    return std::abs(actual - expected) <= tolerance;
}
}

int main() {
    using dsp::targets::daisy::MonotonicMicros;
    using dsp::targets::daisy::TempoController;

    TempoController tempo;
    if (!near(tempo.tempo_bpm(), 120.0F)) return 1;
    if (tempo.midi_authoritative()) return 2;
    if (tempo.transport_running()) return 3;

    tempo.on_midi_start(0);
    constexpr MonotonicMicros interval120 = 20833;
    for (std::uint32_t i = 0; i < 25; ++i) {
        tempo.on_midi_clock(static_cast<MonotonicMicros>(i) * interval120);
    }
    if (!near(tempo.tempo_bpm(), 120.0F, 0.2F)) return 4;
    if (!tempo.midi_authoritative()) return 5;
    if (!tempo.transport_running()) return 6;

    const float before_non_monotonic = tempo.tempo_bpm();
    tempo.on_midi_clock(interval120);
    if (!near(tempo.tempo_bpm(), before_non_monotonic, 0.001F)) return 7;

    tempo.on_midi_stop();
    if (tempo.midi_authoritative()) return 8;
    if (tempo.transport_running()) return 9;

    TempoController taps;
    taps.on_tap(1000000);
    if (!near(taps.tempo_bpm(), 120.0F)) return 10;
    taps.on_tap(1500000);
    if (!near(taps.tempo_bpm(), 120.0F, 0.01F)) return 11;
    taps.on_tap(1699000);  // 199 ms: invalid
    if (!near(taps.tempo_bpm(), 120.0F, 0.01F)) return 12;
    taps.on_tap(4501001);  // > 3000 ms from last valid tap: invalid
    if (!near(taps.tempo_bpm(), 120.0F, 0.01F)) return 13;

    TempoController slow;
    slow.on_midi_start(0);
    constexpr MonotonicMicros interval20 = 125000;
    for (std::uint32_t i = 0; i < 25; ++i) {
        slow.on_midi_clock(static_cast<MonotonicMicros>(i) * interval20);
    }
    if (!near(slow.tempo_bpm(), 20.0F, 0.01F)) return 14;
    const MonotonicMicros last_clock = 24U * interval20;
    slow.update_timeout(last_clock + 749999);
    if (!slow.midi_authoritative()) return 15;
    slow.update_timeout(last_clock + 750001);
    if (slow.midi_authoritative()) return 16;

    TempoController arbitration;
    arbitration.on_midi_start(0);
    for (std::uint32_t i = 0; i < 25; ++i) {
        arbitration.on_midi_clock(static_cast<MonotonicMicros>(i) * interval120);
    }
    const float midi_bpm = arbitration.tempo_bpm();
    arbitration.on_tap(1000000);
    arbitration.on_tap(1500000);
    if (!near(arbitration.tempo_bpm(), midi_bpm, 0.001F)) return 17;
    arbitration.on_midi_stop();
    arbitration.on_tap(2000000);
    arbitration.on_tap(2500000);
    if (!near(arbitration.tempo_bpm(), 120.0F, 0.01F)) return 18;

    TempoController continued;
    for (std::uint32_t i = 0; i < 25; ++i) {
        continued.on_midi_clock(static_cast<MonotonicMicros>(i) * interval120);
    }
    if (continued.midi_authoritative()) return 19;
    continued.on_midi_continue(25U * interval120);
    if (!continued.midi_authoritative()) return 20;

    return 0;
}
