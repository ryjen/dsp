# Faust Tremolo Portable Slice Design

## Goal

Use tremolo to evaluate Faust against the host-independent realtime processor contract from #3, then record an explicit adopt/reject/selective-use decision.

The result must run through the existing native/offline harness, remain allocation-free during `process()`, and keep JUCE, Daisy, MIDI packet formats, and Faust runtime headers out of `dsp_core`.

## Decision summary

Adopt Faust **selectively** as an authoritative source language for effect kernels where concise signal-flow code and portable code generation materially improve testability or target portability.

Do not make Faust the repository's processor abstraction, lifecycle API, control-plane API, or mandatory dependency for ordinary native compilation.

## Architecture

```text
control thread
  rate/depth/shape targets
          |
          v
  TremoloProcessor (native C++)
  - AtomicFloat publication
  - Processor lifecycle
  - preallocated gain buffer
          |
          v
  TremoloFaust generated kernel
  - rate/depth/shape smoothing
  - LFO phase + waveform morph
  - gain signal generation
          |
          v
  native per-channel gain application
          |
          v
      AudioBlock
```

`TremoloProcessor` implements the existing `dsp::Processor` contract. The Faust kernel produces one gain value per frame, so the same modulation phase is applied to every prepared audio channel without duplicating oscillator state.

## Tremolo signal model

The authoritative source is `effects/tremolo/faust/tremolo.dsp`.

Parameters:

- `rate_hz`: 0.1 to 20 Hz, default 4 Hz.
- `depth`: 0 to 1, default 0.5.
- `shape`: 0 to 1, default 0; morphs continuously from sine to triangle.

The Faust source smooths all three UI targets with a fixed, sample-rate-aware one-pole smoothing stage. This avoids block-edge discontinuities while keeping per-frame work bounded.

A common phasor drives both waveform forms. The waveform is bipolar `[-1, 1]`; it is converted to a unipolar modulation signal `[0, 1]`. Gain is `1 - depth + depth * modulation`, so depth zero is exact unity and full depth never phase-inverts the input.

The generated Faust DSP has zero audio inputs and one gain output. Native code multiplies each audio channel by that output.

## Generated-code boundary

`effects/tremolo/generated/tremolo_faust.hpp` is generated output and must not be hand edited.

Generation uses the Faust version pinned by `flake.lock`; the current pinned compiler is Faust 2.85.5. A repository architecture template in `faust/architectures/standalone-header.cpp` inlines the official Faust DSP/UI/Meta interfaces into the generated header. Consequently:

- normal C++ compilation consumes the committed header and does not require Faust to be installed;
- regeneration requires Faust and is explicit;
- CI regenerates to a temporary file and byte-compares it to the committed artifact;
- the generated file retains Faust notices and generation metadata;
- updating Faust intentionally changes generated output and must be reviewed.

The architecture template contains only includes plus Faust's `<<includeIntrinsic>>` and `<<includeclass>>` insertion points. It does not add a second runtime or host abstraction.

## Processor and realtime behavior

`TremoloProcessor::prepare()` validates `ProcessSpec`, initializes the generated Faust object, discovers its three control zones, preallocates a gain buffer sized to `max_block_size`, and publishes the initial targets. Allocation in `prepare()` is allowed.

`process(AudioBlock)` is `noexcept` and performs no allocation, filesystem/network I/O, logging, or blocking synchronization. It loads three `AtomicFloat` targets once per block, writes those targets to the Faust control zones, renders exactly `frame_count` gain samples into the preallocated buffer, then applies the gain to each channel.

The processor supports any positive prepared channel count because the generated kernel is channel-independent. Hosts remain responsible for honoring prepared maximum block and channel counts.

`reset()` clears oscillator/smoothing state deterministically and reapplies the currently published targets.

## Testing and evidence

The existing offline harness is the correctness surface. Tests cover:

- depth zero: exact passthrough;
- silence: remains exactly silent;
- DC input at full depth: bounded modulation with measurable variation;
- sine input: deterministic repeated renders after reset;
- shape endpoints: deterministic but observably different sine/triangle modulation;
- parameter transitions: bounded adjacent-sample discontinuity after a target change;
- parameter clamping: public setters constrain values to documented ranges;
- realtime allocation: the existing allocation detector wraps `process()`;
- callback cost: the existing callback-budget reporter measures representative 48 kHz / 64-frame and 96 kHz / 128-frame configurations.

Floating-point comparisons use the native harness's explicit absolute/relative tolerance helper. Tests must not depend on listening or audio hardware.

## Error policy

Invalid non-realtime configuration is rejected during `prepare()` or clamped by parameter setters before publication. Realtime `process()` does not throw.

If generated UI zone discovery fails during `prepare()`, preparation fails before audio starts. Missing zones are not tolerated silently because that would make generated-source drift invisible.

Normal native compilation must still work from committed generated code if Faust is unavailable. Only regeneration/reproducibility checks require the compiler.

## ADR decision

ADR-0002 records **selective adoption**:

- adopt Faust for suitable portable effect kernels and target generation;
- retain native C++ `Processor`, control publication, offline harness, evidence, and hardware/host adapters;
- do not require every effect to use Faust;
- do not let generated Faust interfaces leak into `dsp_core` or public cross-effect contracts.

JUCE, Daisy firmware, custom hardware, plugin UI, broad effect libraries, and cross-repo protocol work remain outside #4.
