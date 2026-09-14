# ADR-0001: Host-independent realtime processor contract

- Status: Accepted
- Date: 2026-09-14
- Issue: #3

## Context

`ryjen/dsp` needs one processing boundary that can be exercised by deterministic native tests now and adapted to JUCE, Daisy, or other hosts later. The contract must not make a desktop framework, embedded SDK, file format, or MIDI representation part of effect code.

## Decision

Use a small C++20 virtual `Processor` contract over caller-owned planar `float` buffers.

`ProcessSpec` declares sample rate, maximum block size, and channel count. `AudioBlock` is a non-owning view over a host-owned array of channel pointers. Processors may mutate samples in place and must not retain block pointers.

Lifecycle is split deliberately:

- `prepare(ProcessSpec)` runs outside the realtime path and may allocate;
- `reset() noexcept` is bounded;
- `process(AudioBlock) noexcept` is the realtime callback boundary.

Virtual dispatch is accepted because it keeps heterogeneous host integration simple and the dispatch cost is negligible relative to useful block DSP at this stage. Reconsider only if measured callback-budget evidence shows it is material.

## Control state

Control threads publish scalar targets through `AtomicFloat`, implemented with an always-lock-free 32-bit atomic representation. Concrete processors own fixed parameter slots rather than a dynamic registry.

Smoothing is processor responsibility. `LinearSmoother` provides fixed-state sample ramps; a processor reads an atomic target at a deterministic boundary and chooses the smoothing duration appropriate to the parameter.

## Realtime constraints

Realtime code must not allocate/free memory, perform I/O, log, take blocking locks, throw exceptions, or execute work unbounded by prepared configuration.

Each future effect must include an allocation-guard regression test around its `process()` path. The core test proves the provided primitives can be used allocation-free; it cannot guarantee arbitrary processor implementations.

## Native host ownership

`dsp_native` owns all non-realtime concerns used for development evidence: vector allocation, deterministic fixture generation, offline block slicing, tolerance comparison, and wall-clock callback measurement.

Timing reports are evidence, not pass/fail thresholds, until representative target measurements justify stable budgets.

## Consequences

- JUCE/Daisy adapters can wrap the same processor without changing effect code.
- Effects cannot rely on host/framework types.
- File/audio-device concerns stay outside core.
- Planar buffers match common audio APIs and avoid ownership/allocation in the contract.
- A future template/concept path remains possible if profiling proves virtual dispatch significant.
