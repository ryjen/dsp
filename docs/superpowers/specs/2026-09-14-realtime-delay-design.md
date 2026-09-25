# Realtime Delay and Tempo-Sync Design

## Goal

Implement delay as the second portable `dsp::Processor` slice, proving bounded stateful processing, fractional circular-buffer reads, stable feedback, click-resistant live control changes, and normalized tempo synchronization.

The delay remains independent of MIDI packet formats, JUCE, Daisy, and `guitar-practice-system` domain types.

## Decision summary

Use a native C++ implementation rather than Faust. This intentionally exercises ADR-0002's selective-use rule: explicit circular-buffer ownership and realtime invariants are clearer and easier to audit natively.

Use one preallocated ring buffer per prepared audio channel with a shared write index and shared delay/control state.
## Public effect contract

`DelayProcessor` implements the existing `Processor` lifecycle and exposes normalized effect controls only.

```cpp
enum class DelaySubdivision : std::uint32_t {
    free,
    quarter,
    dotted_eighth,
    eighth,
    eighth_triplet,
    sixteenth,
};

class DelayProcessor final : public Processor {
public:
    static constexpr float max_delay_ms = 2000.0F;
    static constexpr float max_feedback = 0.95F;

    bool prepare(const ProcessSpec& spec) override;
    void reset() noexcept override;
    void process(AudioBlock block) noexcept override;
};
```
Controls:

- `set_delay_ms(float)` clamps free delay time to `[1, 2000]` ms.
- `set_feedback(float)` clamps to `[0, 0.95]`.
- `set_mix(float)` clamps to `[0, 1]`.
- `set_tempo_bpm(float)` clamps normalized tempo to `[20, 300]` BPM.
- `set_subdivision(DelaySubdivision)` selects free time or a musical subdivision.

Subdivision resolution is a pure deterministic mapping from BPM to milliseconds:

- quarter = `1.0` beat
- dotted eighth = `0.75` beat
- eighth = `0.5` beat
- eighth triplet = `1/3` beat
- sixteenth = `0.25` beat

At 120 BPM these resolve to 500, 375, 250, 166.666..., and 125 ms respectively. Resolved time is clamped to the legal delay range.
## Prepared state and bounds

`prepare()` allocates all ring storage. Capacity per channel is `ceil(sample_rate * 2.0) + 2` samples, providing the full two-second range plus interpolation safety.

No storage growth is permitted from `reset()` or `process()`. `reset()` zeroes existing buffers, resets the shared write index, and initializes all smoothers directly to the currently published controls.

Delay time is converted to samples and clamped to `[1, capacity - 2]`. Requiring at least one sample of delay avoids reading the slot currently being written and keeps the feedback equation unambiguous even for unusual sample rates.

Each prepared channel has an independent feedback path. Channels share parameters and the write index but never feed audio into each other.
## Realtime algorithm

For each frame:

1. Advance delay-time, feedback, and mix smoothers.
2. Compute a fractional read position `write_index - delay_samples` and wrap it into the ring.
3. Linearly interpolate the two neighboring ring samples for each channel.
4. Write `input + delayed * feedback` into the current ring slot.
5. Output `input * (1 - mix) + delayed * mix`.
6. Advance the shared write index once after all channels process the frame.

The callback performs fixed bounded work proportional only to `frame_count * channel_count`; it allocates no memory, performs no I/O, and takes no blocking locks.
## Parameter transitions

Live delay-time changes use linear fractional-delay interpolation plus a fixed 100 ms `LinearSmoother` ramp. This deliberately produces a bounded tape-style pitch glide rather than an instantaneous read-head jump.

Mix and feedback use 10 ms ramps. A new ramp begins only when the published target actually changes; a constant target does not restart smoothing every block.

Initial preparation/reset does not ramp from zero. Smoothers reset directly to the current targets so deterministic impulse placement is exact from the first processed frame.

The implementation records the last applied targets in prepared state. Tempo or subdivision changes therefore enter the same delay-time ramp as free-time changes.
## Control boundary

The processor consumes normalized BPM, subdivision, and scalar effect parameters. It does not parse MIDI clock, transport, CC, or program-change packets.

A future MIDI/control adapter may derive BPM or parameter targets and call these normalized setters. `DelaySubdivision` is an effect-level timing choice, not a MIDI representation and not a copy of the practice-system score model.

The subdivision selector is published through a fixed-size atomic integer. Scalar controls use `AtomicFloat`; all are loaded once per block before sample processing.

## Failure behavior

`prepare()` returns false for an invalid `ProcessSpec` or any capacity calculation that cannot be represented safely. Allocation failures remain preparation-time failures and are not converted into callback behavior.

After preparation, a block that exceeds prepared frame/channel bounds is ignored rather than indexing outside prepared storage. Null channel pointers are skipped consistently with the existing processor conventions.
## Deterministic evidence

Tests must cover:

- exact impulse placement with integer-sample delay;
- feedback decay at known gain values;
- dry, wet, and mixed signal extremes;
- silence and finite DC behavior;
- free-time min/max clamping;
- feedback/mix/BPM clamping;
- each musical subdivision at a known BPM;
- fractional-delay interpolation with a known short vector;
- deterministic repeat rendering within existing sample tolerances;
- a live delay-time change whose adjacent output discontinuity remains bounded under the documented 100 ms ramp;
- zero callback allocations after preparation;
- callback-budget reports at 48 kHz / 64-frame stereo and 96 kHz / 128-frame stereo.

Timing reports are evidence only, not machine-specific pass/fail thresholds.
## Alternatives considered

**Faust delay kernel:** rejected for this slice. Faust can express delay lines well, but the primary purpose here is to prove explicit prepared memory ownership, buffer bounds, and native stateful-effect conventions.

**Instant delay-time jumps:** rejected because read-head discontinuities can click.

**Dual-head crossfade:** deferred. It can avoid tape-style pitch glide but doubles read work during transitions and adds transition-state complexity that is not required for the first bounded delay.

**Dynamic ring growth:** rejected because callback-time capacity changes violate realtime invariants.

## Non-goals

- ping-pong or cross-channel feedback;
- modulation/chorus behavior;
- filters in the feedback path;
- MIDI clock decoding;
- tap-tempo gesture detection;
- preset persistence;
- hardware controls or pedal firmware.
## Acceptance mapping

This design satisfies issue #5 when:

- all delay storage is allocated during `prepare()` and allocation guards prove callback stability;
- impulse and feedback tests prove exact ring-buffer behavior;
- illegal parameters are clamped before they can escape prepared bounds;
- delay-time transitions use the documented fractional 100 ms ramp;
- subdivision-to-millisecond mapping is deterministic and separately testable;
- public/source coupling checks remain free of MIDI packet, JUCE, and Daisy types;
- representative callback-budget reports are produced through the existing native harness.