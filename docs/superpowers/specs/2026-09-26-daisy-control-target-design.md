# Daisy Pedal Target and Realtime Control Design

- **Status:** Proposed — awaiting written-spec review
- **Date:** 2026-09-26
- **Issue:** #6
- **Parent:** #1
- **Depends on:** #3, #4, #5

## Intent

Run the existing host-independent DSP processors on a Daisy Seed-class pedal target while preserving the repository boundary already established by the native harness, tremolo, and delay slices.

The target must prove that the same portable processor implementations can execute behind a hardware adapter, while physical controls, MIDI parsing, transport state, bypass behavior, and libDaisy-specific types remain outside `core/` and `effects/`.

The reference hardware proof uses the existing `DelayProcessor` because it exercises stateful memory, tempo synchronization, feedback, live control changes, and bypass in one slice. Tremolo remains portable through the same `Processor` boundary but is not required to close #6.

This slice establishes the realtime/hardware boundary. It does **not** define the external `guitarctl` preset/configuration protocol; that remains #7.

## Selected first target

Use the current stereo Cleveland Music Co. Hothouse carrier with Daisy Seed or Seed3 as the first pedal target.

The current Hothouse is explicitly Seed3-compatible, provides stereo audio I/O, six potentiometers, three toggle switches, two momentary footswitches, and two LEDs. It therefore provides enough controls and audio plumbing to validate the runtime without designing a custom PCB or enclosure.

Reference:
- https://clevelandmusicco.com/products/hothouse-digital-signal-processing-platform-kit
- https://docs.daisy.audio/tutorials/_a3_Getting-Started-Audio/

The firmware target must not assume every future Daisy carrier has the Hothouse layout. Hothouse pin/control knowledge stays under `targets/daisy/`.

## Architecture

```text
                     portable DSP
                  DelayProcessor
                        ^
                        | normalized state
                        |
               control application layer
            +-----------+-------------+
            |           |             |
         tempo       parameter      bypass
         state        updates        state
            ^           ^             ^
            |           |             |
       MIDI adapter   pots/toggle   footswitch
            ^           ^             ^
            +-----------+-------------+
                        |
                 targets/daisy/
                        |
              libDaisy + Hothouse
```

Dependency direction is one-way:

```text
core + effects
      ^
      |
portable control contracts/state
      ^
      |
Daisy/Hothouse adapters
```

`core/` and `effects/` must contain no libDaisy headers, Daisy SDK types, Hothouse pin definitions, MIDI packet types, filesystem/network APIs, or hardware logging.

## Component boundaries

### 1. Portable control model

Add a small fixed-state control model using value types only. It carries normalized intent, not hardware protocol details.

Use one fixed control snapshot rather than parameter IDs or a generic registry:

```cpp
struct PedalControlSnapshot {
    float primary_normalized;    // 0..1
    float secondary_normalized;  // 0..1
    float tempo_bpm;             // 20..300
    std::uint8_t mode;           // bounded 3-position selector
    bool bypassed;
    bool transport_running;
};
```

The control/main loop is the single writer. It validates and normalizes ADC, footswitch, tap-tempo, and MIDI input, then publishes each scalar field through fixed lock-free storage using the repository's existing `AtomicFloat` / lock-free integer atomics. The audio callback snapshots every published field once at the start of a block and uses that immutable block-local copy for the entire callback.

There is no event queue in the audio path, no heap ownership, no dynamic parameter registry, and no numeric parameter-ID namespace. Continuous controls are latest-value-wins state. Momentary controls are converted by the single writer into final state before publication, for example a debounced footswitch toggles the published `bypassed` boolean.

Concrete effect bindings may know the effect type. The #6 reference binding maps the fixed snapshot to the existing `DelayProcessor` setters. Effect processors never parse raw MIDI or inspect Daisy types.

### 2. MIDI boundary

Raw MIDI transport and packet parsing live under the Daisy target adapter. USB receive/parsing callbacks do not mutate processor/control state directly; parsed events are drained and validated by the single-writer control/main loop before publication.

libDaisy currently provides `MidiHandler` with UART and USB transports; the transport/parser produces MIDI events before project code translates them into normalized controls.

References:
- https://docs.daisy.audio/libDaisy/midi_8h/
- https://docs.daisy.audio/libDaisy/classdaisy_1_1MidiUartTransport/
- https://docs.daisy.audio/libDaisy/classdaisy_1_1MidiUsbTransport/

The first physical proof uses **USB MIDI** through libDaisy's USB MIDI transport. UART MIDI is not required for #6.

The target supports:

- MIDI clock -> normalized tempo observations;
- Start / Stop / Continue -> transport state;
- Control Change on MIDI channel 1 only:
  - CC 20 -> primary control / free delay time;
  - CC 21 -> secondary control / feedback.

Other channel-voice messages, channels, CC numbers, SysEx, and Program Change are ignored in #6. Persistent or externally selected presets remain #7.

MIDI clock timing logic must be deterministic and separately testable. MIDI Clock is interpreted at 24 pulses per quarter note. After at least two valid observations, BPM derives from elapsed monotonic time and is clamped to 20–300 BPM. A bounded moving estimate over the most recent 24 pulse intervals is permitted to reduce jitter; storage must be fixed-size and callback-allocation-free.

Malformed, truncated, unsupported, or out-of-range input must fail closed at the adapter/translation boundary. Rapid valid control input must remain bounded, allocation-free, and exception-free.

### 3. Tempo ownership

Tempo has exactly two local sources in #6: valid running MIDI clock and physical tap tempo.

Authority is deterministic:

1. A valid running MIDI clock has priority.
2. MIDI becomes authoritative after Start or Continue once a valid BPM estimate is available.
3. MIDI remains authoritative while transport is running and clock observations remain fresh.
4. A MIDI Stop, or clock timeout while running, releases MIDI authority.
5. Tap tempo is accepted only while MIDI is not authoritative.
6. Once accepted, tap tempo remains the active BPM until a valid running MIDI clock retakes authority or a later valid tap replaces it.
7. If neither source has produced a valid BPM, the target starts at 120 BPM.

Clock freshness uses elapsed monotonic time, never callback counts. After a valid estimate exists, MIDI authority expires when no clock pulse arrives for `max(250 ms, 6 * expected_clock_interval)`. This remains tolerant at the 20 BPM lower bound while releasing authority promptly at faster tempos.

Tap tempo accepts intervals corresponding to 20–300 BPM (200–3000 ms). Out-of-range taps are ignored. Two valid taps establish a tempo; later valid taps update it using a fixed-size average of at most the last four intervals.

Tempo publication is bounded to the existing delay range of 20–300 BPM. Source changes publish normalized BPM state; they never directly mutate delay-line memory.

### 4. Physical controls

The reference Hothouse/Delay mapping is explicit:

- Pot 1 -> free delay time using an exponential map from 1 ms to 2000 ms, preserving useful resolution at short musical delays;
- Pot 2 -> feedback using a linear map from 0.0 to 0.95;
- Toggle 1 -> free / quarter-note / dotted-eighth subdivision;
- Footswitch 1 -> bypass toggle;
- Footswitch 2 -> tap tempo.

The same normalized mappings are used by MIDI CC 20 and CC 21, so physical and MIDI control paths share one binding implementation.

The remaining four pots, two toggles, and two LEDs are outside the required proof unless needed for a bounded diagnostic indication.

When Toggle 1 selects a synchronized subdivision, Pot 1 continues publishing the remembered free-time value but that value is not the effective delay time until free mode is selected again. This preserves the existing `DelayProcessor` semantics rather than inventing target-specific delay behavior.

Physical controls are sampled/debounced outside the sample-processing inner loop. Footswitch/toggle edges use a 20 ms debounce window. Pot changes publish only when normalized movement is at least 0.002, preventing ADC jitter from continuously restarting the delay processor's smoothing ramps. Their normalized state is then published through the fixed lock-free handoff and snapshotted by the audio callback at block boundaries.

The exact primary mapping is `delay_ms = exp(log(2000.0F) * normalized)`, producing 1 ms at 0 and 2000 ms at 1. Feedback is `0.95F * normalized`. MIDI CC values map from `0..127` to normalized `0..1` before the same binding functions are called.

### 5. Daisy audio adapter

The Daisy target owns conversion between libDaisy audio callback buffers and the existing `dsp::AudioBlock` / `Processor` contract.

The target must run the existing `DelayProcessor` implementation already exercised by native tests. No Daisy-specific fork of delay is permitted.

The expected callback shape is:

```text
Daisy input buffers
      |
      | copy/alias using prepared fixed storage
      v
existing DelayProcessor
      |
      v
bypass crossfade / final output
      |
      v
Daisy output buffers
```

No allocation, filesystem/network I/O, logging, blocking locks, or unbounded parsing may occur in the audio callback.

libDaisy supports configurable block size and sample rate; the target records the actual runtime configuration rather than embedding those values in portable DSP contracts.

Reference:
- https://docs.daisy.audio/tutorials/_a3_Getting-Started-Audio/

The prototype firmware configures **48 kHz / 48 frames**, yielding a 1 ms nominal callback deadline. These are target settings, not core-DSP constants. The hardware evidence record must confirm the actual sample rate, block size, callback period, and measured CPU/callback headroom rather than assuming configuration succeeded.

### 6. Bypass semantics

Bypass belongs to the pedal/runtime adapter, not `DelayProcessor`.

The first implementation uses software dry-through with a short bounded gain crossfade between dry and processed paths. This prevents abrupt switching discontinuities without requiring hardware-relay true bypass.

Conceptually:

```cpp
copy_input_to_prepared_dry_scratch();
copy_input_to_output_block();
processor.process(output_block);
crossfade(prepared_dry_scratch, output_block, bypass_mix);
```

Because the existing `Processor` contract is in-place, the runtime allocates dry scratch during `prepare()` for exactly `channel_count * max_block_size` samples. The callback never resizes it. Bypass transitions use a 5 ms sample ramp between processed and dry states; repeated publication of the same bypass state does not restart the ramp.

The processor continues running and receiving input while bypassed. Bypass therefore affects only the output mix: delay state/tails remain continuous and re-engaging the effect does not reset or cold-start processor state.

Hardware relay bypass is outside this slice.

### 7. Denormals/subnormals

Do not assume denormal handling.

Before #6 is considered complete, the target must document the selected runtime policy:

1. establish whether the Daisy/libDaisy runtime enables a supported flush-to-zero mode; or
2. explicitly enable the appropriate target policy during initialization; or
3. if neither is reliable, add narrowly-scoped suppression where stateful DSP requires it and document that decision separately.

A decaying feedback-path hardware test must verify that very small state does not create callback-time spikes or non-finite output.

This concern must not leak Daisy-specific floating-point control code into portable effects unless evidence shows a portable suppression policy is required.

## Runtime and failure model

The firmware is intentionally local and bounded:

- no direct dependency on `ryjen/guitar-practice-system`;
- no network or filesystem mutation;
- no shared mutable cross-repository state;
- no dynamic configuration fetch during audio processing;
- invalid input cannot write arbitrary processor state;
- unsupported Program Change or CC values are ignored deterministically;
- loss of MIDI authority follows the tempo-ownership rules above;
- processor preparation failure prevents audio processing from starting rather than continuing with partially initialized state.

A firmware failure therefore cannot silently mutate practice-system state. Future #7 integration may provide an explicit versioned configuration contract, but that boundary remains additive.

## Repository layout

Target direction:

```text
core/
  include/dsp/control/          # small portable control value/state types

targets/
  daisy/
    include/dsp/targets/daisy/
      control_mapper.hpp
      midi_adapter.hpp
      pedal_runtime.hpp
    src/
      control_mapper.cpp
      midi_adapter.cpp
      pedal_runtime.cpp
    firmware/
      hothouse_main.cpp
    README.md

test/
  control/
  targets/daisy/
```

Exact filenames may change if existing project conventions suggest a clearer split, but target-specific code remains isolated under `targets/daisy/`.

## Build and dependency policy

Follow the repository/Micrantha CI rule: dependencies belong to the repository flake, not a feature-heavy runner image.

The implementation should pin the ARM cross-toolchain and libDaisy in `flake.nix` or repository-local build inputs. Hosted CI must be able to compile the firmware target without requiring a globally provisioned Daisy SDK.

`libDaisy` is MIT-licensed and is an acceptable firmware dependency. The official `clevelandmusicco/HothouseExamples` repository is GPL-3.0; it may be consulted as documentation/reference, but #6 must not copy from it, link it, vendor it, or make it a build dependency unless a separate licensing decision explicitly changes the repository's distribution obligations. The Hothouse adapter should therefore be implemented directly against libDaisy plus published hardware/pin information.

Do not introduce a source dependency from `guitar-practice-system`. Do not add JUCE to this slice.

A libDaisy submodule is not preferred merely for convenience; use a pinned repository-local dependency mechanism unless the upstream build structure makes a submodule materially simpler and that trade-off is documented.

## Verification strategy

### Hosted/native CI

Hardware-independent behavior must be deterministic and testable in ordinary CI:

- MIDI event -> normalized control translation;
- MIDI clock observations -> bounded BPM;
- Start / Stop / Continue and clock-timeout authority transitions;
- CC normalization and out-of-range rejection;
- unsupported Program Change and non-channel-1 CC messages are ignored;
- physical-control normalization and exact exponential/linear mappings;
- exact Pot 1 / Pot 2 / Toggle 1 / footswitch mapping;
- tap-tempo state and MIDI-priority arbitration;
- bypass ramp/crossfade behavior;
- rapid control-change stress without callback allocation;
- malformed/unsupported control input fails closed;
- compile-time/core-boundary checks proving no Daisy/libDaisy types enter `core/` or `effects/`;
- firmware target cross-compiles from pinned repo dependencies;
- cross-compiled firmware configures 48 kHz / 48-frame audio and USB MIDI;
- a compile-time/static boundary test rejects any Daisy/libDaisy include under `core/` or `effects/`.

### Hardware smoke gate

The following claims require a real Daisy/Hothouse-class target and must not be fabricated:

- firmware flashes and boots;
- stereo/mono audio I/O behaves as configured;
- Pot 1, Pot 2, and Toggle 1 affect the existing `DelayProcessor` as specified;
- footswitch bypass works without an obvious switching artifact;
- tap tempo affects tempo-synchronized delay;
- MIDI transport/control works through the selected physical transport;
- actual sample rate and block size are recorded;
- callback/CPU headroom is measured under representative processing with libDaisy's `CpuLoadMeter` or equivalent timing evidence, with reporting performed outside the audio callback;
- decaying feedback does not exhibit denormal-related callback spikes.

If hardware is not available, implementation may merge the portable/control and cross-compilation portions only if #6 remains open with the hardware acceptance items explicitly unchecked.

## Acceptance mapping

Issue #6 is complete only when:

- the existing portable `DelayProcessor` runs on the selected Daisy target;
- `core/` and `effects/` contain no Daisy SDK types/includes;
- MIDI parsing/translation remains outside effect implementations;
- Pot 1, Pot 2, Toggle 1, and the footswitches safely drive the specified state;
- bypass semantics are implemented and verified;
- actual hardware sample-rate/block-size/headroom evidence is recorded;
- malformed control input and rapid valid changes remain bounded;
- denormal/subnormal policy is documented and hardware-checked for stateful feedback;
- firmware has no hidden dependency on or mutation path into `guitar-practice-system`.

## Deferred work

- #7 versioned `guitarctl` -> DSP preset/control protocol;
- persistent/external preset storage;
- custom pedal PCB/enclosure;
- hardware-relay true bypass;
- general multi-board hardware abstraction framework;
- JUCE host integration (#9);
- dynamics/phrasing analysis (#8).

## Design consequences

This design adds one reusable seam: normalized realtime control intent between hardware/protocol adapters and portable DSP.

It deliberately does not generalize beyond evidence needed by the first target. Hothouse/Daisy-specific wiring can change without changing processor contracts; MIDI transports can change without effects knowing; and #7 can later add an external control source without replacing the hardware or DSP architecture.
