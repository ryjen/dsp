# ADR 0007: Retain JUCE as an optional host adapter only

- **Status:** Accepted
- **Date:** 2026-10-11
- **Issue:** #9

## Context

The host-independent `Processor` contract, native/offline correctness harness,
portable tremolo/delay effects, Daisy target, and bounded analyzer all exist without
a desktop application framework.

JUCE can still be useful for interactive desktop audio-device and future plugin-host
experiments, but adopting JUCE as the DSP abstraction would duplicate already-proven
contracts and couple embedded/core correctness to a large host framework.

Licensing is also a distribution concern. As evaluated on 2026-10-11, upstream JUCE
9 modules are dual-licensed under AGPLv3 or the commercial JUCE licence. The
repository's pinned `nixos-26.05` package currently provides JUCE 8.0.12 and marks
that packaged source AGPL-3.0-only. This repository remains MIT-licensed, so no
JUCE-linked binary should be published from this repository without a separate,
explicit licensing/distribution decision.

## Decision

Retain JUCE as an **optional host adapter**.

- Ordinary CMake builds keep `DSP_ENABLE_JUCE_HOST=OFF`.
- Core, effects, analysis, protocols, native tests, and Daisy firmware do not find,
  include, or link JUCE.
- `hosts/juce/ProcessorAudioSource` decorates a JUCE `AudioSource`: the upstream
  source fills a buffer, then the adapter passes that buffer through the unchanged
  `dsp::Processor` interface.
- The adapter owns no effect implementation and introduces no JUCE type into the
  processor contract.
- A separate Nix `juce-host` check enables the adapter against the JUCE package
  pinned by nixpkgs. A dedicated `nix develop .#juce` shell exposes the optional
  dependency for local experiments.
- A source-boundary check rejects JUCE includes/types from core/effects/analysis,
  protocols, and embedded targets.
- The native/offline harness remains the authoritative deterministic correctness
  path.

## Evidence

The evaluation test runs the existing `TremoloProcessor` unchanged behind the JUCE
adapter. At zero depth it remains exact passthrough; at full depth it produces
bounded modulation. The adapter itself performs no heap allocation while processing
an already-prepared block.

This is enough to prove the integration seam without building a GUI, plugin binary,
or audio-device product.

## Supported use cases

Keep JUCE available for:

- local interactive audio-device experiments;
- future desktop parameter-control prototypes;
- future plugin-host experiments where JUCE materially reduces host boilerplate.

Do not use JUCE for:

- core/effect APIs;
- deterministic golden/vector correctness;
- Daisy/embedded runtime;
- required command-line tooling;
- release binaries unless licensing is revisited explicitly.

## Consequences

JUCE can be upgraded or removed without changing portable DSP code. Upstream JUCE
may move faster than the nixpkgs package used by this repository; that is acceptable
because the adapter uses only the stable `AudioSource` / `AudioBuffer` surface.

A future actual plugin or distributed desktop application requires a new decision
covering current JUCE version, licence choice, formats, signing, packaging, and
distribution.
