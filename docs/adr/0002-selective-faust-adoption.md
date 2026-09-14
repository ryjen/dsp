# ADR-0002: Selective Faust adoption

- **Status:** Accepted
- **Date:** 2026-09-14
- **Issue:** #4

## Context

The realtime processor contract in ADR-0001 deliberately keeps effect math independent of hosts and hardware targets. Issue #4 used tremolo to evaluate whether Faust should become part of that architecture or remain an implementation tool.

The evaluation required deterministic offline rendering, bounded parameter smoothing, allocation-free prepared processing, reproducible generated code, and ordinary native builds that do not require Faust.
## Decision

Adopt Faust **selectively** as an authoritative source language for DSP kernels where its signal-flow model improves clarity, testability, or target portability.

Keep the repository's public realtime architecture native C++:

- `dsp::Processor`, `ProcessSpec`, `AudioBlock`, and control publication remain native contracts;
- host, MIDI, hardware, and evidence adapters do not depend on Faust types;
- an effect may remain hand-written C++ when Faust adds no material value;
- generated Faust types remain private implementation details and never appear in public effect headers.
Faust generation is reproducible and explicit:

- `.dsp` source is committed;
- generated C++ is committed but not hand maintained;
- the Faust compiler version is pinned through `flake.lock`;
- `tools/generate-faust.sh` is the canonical generation entry point;
- `checks.faust-generated` regenerates and byte-compares the committed output;
- `checks.native` consumes committed generated code without adding Faust to its build inputs.

The first kernel emits a channel-independent tremolo gain stream. `TremoloProcessor` applies that stream to prepared channels through the same `Processor` boundary used by the offline harness.
## Generation boundary

Do not use Faust's whole-file C++ namespace option for committed standalone headers. In this evaluation, `-ns` wrapped standard-library `#include`s inside the generated namespace and caused pathological C++ compilation behavior.

Instead, the standalone architecture keeps generated support types at global scope inside a header that is included by exactly one private effect translation unit. Public headers expose only native project types. If future generation needs stronger symbol isolation, use a dedicated generated translation unit or an explicit bridge rather than wrapping standard headers in a namespace.

## Consequences

Benefits:

- concise authoritative signal-flow source;
- deterministic generated artifacts;
- a plausible path to Faust-supported embedded targets including Daisy;
- no Faust installation required by ordinary native consumers.
Costs and constraints:

- generated code changes when the pinned Faust toolchain changes and must be reviewed;
- generated interfaces require an adapter to native controls and lifecycle;
- generated code is not the place for repository-level control, host, or hardware abstractions;
- build-time generation remains a distinct check rather than part of every native compile.

## Evidence

The tremolo slice proves rate/depth/shape controls, deterministic sine/DC/silence behavior, waveform distinction, smoothed live parameter changes, and zero allocations across repeated prepared callbacks.

Callback measurements are reported as evidence, not portability thresholds. On the implementation host the representative runs were approximately 3.94 µs average for 48 kHz / 64-frame stereo and 6.53 µs for 96 kHz / 128-frame stereo.

## Alternatives rejected

**Faust as the repository processor ABI:** rejected because it would leak generator/runtime concerns into host-independent contracts and constrain non-Faust effects.

**Hand-written C++ only:** rejected because Faust gives a concise, reproducible source for suitable DSP kernels and a useful embedded-generation path.

**Runtime or host-time Faust compilation:** rejected for the initial system because deterministic committed output and offline ownership are more important than dynamic DSP compilation.