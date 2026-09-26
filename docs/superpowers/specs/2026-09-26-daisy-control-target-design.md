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

Representative concepts:

```cpp
struct TempoState {
    float bpm;
    bool running;
};

struct ParameterChange {
    std::uint16_t id;
    float normalized;
};

enum class TransportAction : std::uint8_t {
    start,
    stop,
    continue_playback,
};

enum class FootswitchAction : std::uint8_t {
    bypass_toggle,
    tap,
};
```

The exact type layout may vary during implementation, but the following constraints are architectural:

- no heap ownership;
- no dynamic parameter registry;
- bounded, trivially movable/copyable event/state objects;
- normalized scalar values are clamped before publication;
- invalid or unsupported controls are ignored or rejected before reaching processors;
- effect processors never parse raw MIDI or inspect Daisy types.

Concrete effect bindings may know the effect type. The #6 reference binding targets `DelayProcessor`; it maps normalized controls to existing delay setters. #6 does not introduce a generic reflection-based parameter system.

### 2. MIDI boundary

Raw MIDI transport and packet parsing live under the Daisy target adapter.

libDaisy currently provides `MidiHandler` with UART and USB transports; the transport/parser produces MIDI events before project code translates them into normalized controls.

References:
- https://docs.daisy.audio/libDaisy/midi_8h/
- https://docs.daisy.audio/libDaisy/classdaisy_1_1MidiUartTransport/
- https://docs.daisy.audio/libDaisy/classdaisy_1_1MidiUsbTransport/

The first target supports:

- MIDI clock -> normalized tempo observations;
- Start / Stop / Continue -> transport state;
- Control Change -> bounded normalized parameter changes;
- Program Change -> bounded program-selection intent only.

Program Change does not introduce persistent preset storage or the #7 external preset schema. The translator surfaces a bounded program index; the #6 reference firmware is permitted to ignore program indices that have no built-in mapping.

MIDI clock timing logic must be deterministic and separately testable. Clock timing state accepts monotonic timing observations and derives a bounded BPM estimate without allocating in the realtime path.

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

The freshness timeout must be defined in elapsed monotonic time and remain valid across the supported 20–300 BPM range. The implementation plan must derive and test the exact timeout rather than using callback counts.

Tempo publication is bounded to the existing delay range of 20–300 BPM. Source changes publish normalized BPM state; they never directly mutate delay-line memory.

### 4. Physical controls

The reference Hothouse/Delay mapping is explicit:

- Pot 1 -> free delay time, mapped into `DelayProcessor::set_delay_ms`;
- Pot 2 -> feedback, mapped into `DelayProcessor::set_feedback`;
- Toggle 1 -> free / quarter-note / dotted-eighth subdivision;
- Footswitch 1 -> bypass toggle;
- Footswitch 2 -> tap tempo.

The remaining five pots/toggles/LED behavior is outside the required proof unless needed for a bounded diagnostic indication.

When Toggle 1 selects a synchronized subdivision, Pot 1 continues publishing the remembered free-time value but that value is not the effective delay time until free mode is selected again. This preserves the existing `DelayProcessor` semantics rather than inventing target-specific delay behavior.

Physical controls are sampled/debounced outside the sample-processing inner loop. Their normalized state is published through bounded lock-free/fixed-state handoff and snapshotted by the audio callback at block boundaries.

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

The prototype baseline is 48 kHz. Block size is target configuration, not a core-DSP constant. The hardware evidence record must capture the actual sample rate, block size, callback period, and measured CPU/callback headroom.

### 6. Bypass semantics

Bypass belongs to the pedal/runtime adapter, not `DelayProcessor`.

The first implementation uses software dry-through with a short bounded gain crossfade between dry and processed paths. This prevents abrupt switching discontinuities without requiring hardware-relay true bypass.

Conceptually:

```cpp
processed = processor(input);
output = crossfade(dry_input, processed, bypass_mix);
```

The callback must preserve access to the dry input while processing the output buffer. Crossfade state is fixed-size and prepared ahead of callback execution.

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

The implementation should pin the ARM cross-toolchain and libDaisy/Hothouse build dependencies in `flake.nix` or repository-local build inputs. Hosted CI must be able to compile the firmware target without requiring a globally provisioned Daisy SDK.

Do not introduce a source dependency from `guitar-practice-system`. Do not add JUCE to this slice.

A libDaisy submodule is not preferred merely for convenience; use a pinned repository-local dependency mechanism unless the upstream build structure makes a submodule materially simpler and that trade-off is documented.

## Verification strategy

### Hosted/native CI

Hardware-independent behavior must be deterministic and testable in ordinary CI:

- MIDI event -> normalized control translation;
- MIDI clock observations -> bounded BPM;
- Start / Stop / Continue and clock-timeout authority transitions;
- CC normalization and out-of-range rejection;
- bounded Program Change handling;
- physical-control normalization;
- exact Pot 1 / Pot 2 / Toggle 1 / footswitch mapping;
- tap-tempo state and MIDI-priority arbitration;
- bypass ramp/crossfade behavior;
- rapid control-change stress without callback allocation;
- malformed/unsupported control input fails closed;
- compile-time/core-boundary checks proving no Daisy/libDaisy types enter `core/` or `effects/`;
- firmware target cross-compiles from pinned repo dependencies.

### Hardware smoke gate

The following claims require a real Daisy/Hothouse-class target and must not be fabricated:

- firmware flashes and boots;
- stereo/mono audio I/O behaves as configured;
- Pot 1, Pot 2, and Toggle 1 affect the existing `DelayProcessor` as specified;
- footswitch bypass works without an obvious switching artifact;
- tap tempo affects tempo-synchronized delay;
- MIDI transport/control works through the selected physical transport;
- actual sample rate and block size are recorded;
- callback/CPU headroom is measured under representative processing;
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
