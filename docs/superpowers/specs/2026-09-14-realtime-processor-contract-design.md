# Realtime DSP Processor Contract Design

## Context

Issue #3 establishes the first reusable API in `ryjen/dsp`. Effects, analyzers, desktop hosts, and embedded targets will depend on this boundary, so the contract must remain smaller than any one host or effect.

The design follows the repository split established by #2: core code owns realtime-safe data/control primitives; native host code owns buffers, timing, fixture generation, comparison, and reporting. JUCE, Daisy, MIDI packet formats, filesystems, and the historical Go kata stay outside core.

## Chosen approach

Use a small virtual C++20 processor contract with non-owning planar audio blocks. Lifecycle/configuration happens before realtime processing. Realtime entry points are `noexcept` and operate only on caller-owned memory.

Alternative approaches considered:

1. **Template/concept-only processors.** Removes virtual dispatch but makes heterogeneous host integration and generated DSP adapters more complex. Defer this optimization until evidence shows virtual dispatch matters.
2. **Framework-owned blocks (JUCE/Daisy types).** Convenient initially but reverses the dependency direction and would couple every effect to a host. Reject.
3. **Interleaved owning buffers in core.** Simple for file rendering but introduces ownership/allocation semantics into the realtime contract. Reject.

## Core audio contract

`ProcessSpec` contains the prepared sample rate, maximum block size, and channel count. It is validated before processing.

`AudioBlock` is a non-owning planar view:

- `float* const* channels`
- channel count
- frame count

The host owns channel buffers and the pointer array. A processor may mutate samples in place but must not retain block pointers after `process()` returns.

`Processor` exposes:

- `prepare(ProcessSpec)` outside the realtime path; allocation is allowed here
- `reset() noexcept`
- `process(AudioBlock) noexcept`

`process()` receives blocks no larger than the prepared maximum and with the prepared channel count. Host validation happens before invoking it; malformed realtime blocks are contract violations rather than exception paths.

## Control handoff

Core provides an `AtomicFloat` target primitive using a lock-free 32-bit atomic representation. A control thread may publish a target while the audio thread reads it at a block boundary without a mutex or allocation.

Core also provides a fixed-state linear smoother. Control publication and smoothing are separate responsibilities:

1. host/control code publishes a new target;
2. processor reads the target at a deterministic boundary;
3. processor smooths changes over a chosen number of samples where abrupt transitions would click.

This avoids a generic dynamic parameter registry. Effects define their own typed parameter identifiers and fixed sets of atomic target slots.

## Realtime rules

The realtime path must not:

- allocate or free dynamic memory;
- perform filesystem, network, console, or logging I/O;
- take blocking locks;
- throw exceptions;
- execute unbounded loops based on external input.

`prepare()` may allocate and reject invalid configuration. `reset()`, parameter reads, and `process()` are bounded and `noexcept`.

## Native/offline host

`hosts/native` owns deterministic non-realtime utilities:

- allocate planar input/output buffers;
- split a render into blocks no larger than the prepared maximum;
- generate simple deterministic fixtures such as silence, impulse, DC, and sine;
- compare sample vectors with explicit absolute and relative tolerances;
- measure elapsed callback time and derive per-block budget/headroom reports.

The renderer invokes exactly the same `Processor::process()` entry point that later JUCE and Daisy adapters will call.

No wall-clock performance threshold is a correctness test in #3. Timing is reported as evidence; later effects may establish regression budgets after representative hardware/runtime measurements exist.

## Allocation verification

A test-only allocation guard instruments global allocation while the realtime callback is executing. A simple test processor exercises the core block/control/smoothing path. The test fails if `process()` performs dynamic allocation.

This proves the provided primitives and reference usage can be allocation-free; it does not claim arbitrary future processor implementations are automatically safe. Future effects must repeat the guard in their own tests.

## Golden/vector policy

Comparisons use both absolute and relative tolerance:

`abs(actual - expected) <= abs_tol + rel_tol * abs(expected)`

Default test values are explicit at each call site; there is no hidden global tolerance. NaN and infinity compare only when exactly expected by a dedicated test case.

Golden correctness uses deterministic generated fixtures, so CI does not depend on audio hardware or large binary assets.

## Failure policy

- Invalid `ProcessSpec` is rejected before processing.
- Native host utilities return structured status/results for ordinary invalid input.
- Realtime methods do not throw or log.
- Debug assertions may document programmer contract violations, but release correctness must not depend on assertions firing.

## Dependency direction

```text
core contracts + realtime primitives
            ^
            |
     processor implementations
            ^
            |
  +---------+---------+
  |                   |
native host       future hosts/targets
                    JUCE / Daisy
```

Core has no dependency on native host utilities. Hosts depend on core, never the reverse.

## Initial files

```text
core/include/dsp/
  audio_block.hpp
  process_spec.hpp
  processor.hpp
  atomic_float.hpp
  linear_smoother.hpp
  core.hpp
hosts/native/include/dsp/native/
  offline_renderer.hpp
  sample_compare.hpp
  callback_budget.hpp
hosts/native/src/
  offline_renderer.cpp
  callback_budget.cpp
test/core/
test/native/
docs/adr/0001-realtime-processor-contract.md
```

## Acceptance mapping

- Host-independent processor contract: core headers only, no host SDK includes.
- No normal callback allocation: allocation-guard test.
- Bounded realtime parameter updates: lock-free atomic target plus fixed-state smoother.
- Deterministic offline rendering: native renderer plus generated fixtures.
- Golden/vector comparison: explicit tolerance helper and tests.
- Callback budget reporting: native timing utility; no flaky threshold gate.
- No JUCE/Daisy dependency: CMake/Nix/CI continue to build with neither present.
- Dependency rationale: ADR records this design and future extension rules.
