# ADR 0006: Bounded performance observations

- **Status:** Accepted
- **Date:** 2026-10-11
- **Issue:** #8

## Context

Realtime guitar analysis can provide useful measurements to the control plane, but
measurement and practice interpretation have different trust boundaries.

The DSP runtime can reliably observe signal-level events. It cannot infer mastery,
musical correctness, note identity, readiness, or pedagogical action from those
measurements without introducing opaque policy into the realtime plane.

The audio callback also cannot allocate, log, serialize JSON, or block on a consumer.

## Decision

Add a dedicated `analysis/` module, separate from `effects/`, with a first
mono-input `PerformanceAnalyzer`.

The analyzer uses an envelope follower with onset/release hysteresis and emits
`PerformanceObservationV1` values through a fixed-capacity SPSC queue.

Version 1 observations contain only bounded measurements:

- start/end sample frame;
- start timestamp derived from the prepared sample rate;
- peak linear amplitude scaled to milli-units;
- attack duration;
- event duration;
- optional inter-event spacing;
- quality flags.

Quality flags currently identify clipped, too-short, and non-finite-input events.
They represent detector quality/ambiguity, not probability of musical correctness.
An observation with no quality flags is only a detector-quality statement.

## Realtime handoff

The observation queue has a fixed capacity of 64 entries. The callback never waits
for a consumer. If the queue is full, the new observation is dropped and an explicit
lock-free dropped-observation counter is incremented.

No JSON encoding, filesystem/network I/O, logging, heap allocation, practice
assessment, adaptation, note detection, or transport framing occurs in the callback.

## Detection scope

The first slice intentionally supports one mono channel. Stereo/channel-selection,
accent-to-beat analysis, rhythmic drift, dynamic-range windows, and note/pitch
inference are deferred until concrete fixtures justify their reliability.

The envelope detector is deterministic over explicit samples and sample rate. Offline
tests use a synthetic attack/sustain/release fixture to pin attack, duration, peak,
and inter-event timing.

## Control-plane boundary

`guitar-practice-system` owns any later mapping from these measurements into its
evidence records. Its deterministic assessment model distinguishes explicit
observations from confidence, interpretation, recommendation, and approved findings.

DSP therefore emits measurements and quality metadata only. It does not emit
assessment gate results or practice recommendations.

## Consequences

The fixed C++ observation struct is versioned independently from future transport
serialization. A transport adapter may serialize observations outside the callback,
but transport schema changes must not leak into detector state or realtime work.
