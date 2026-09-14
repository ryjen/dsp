# Realtime DSP Bootstrap Design

## Goal

Turn `ryjen/dsp` from a historical Go DSP kata into a reproducible repository foundation for future realtime guitar DSP work without prematurely defining the processor API owned by issue #3.

## Scope

Issue #2 is infrastructure and repository architecture only. It preserves the legacy kata, establishes the new source-tree boundary, adds a native C++ build/test baseline, a Nix development environment, and CI. It does not implement an effect, realtime processor contract, JUCE integration, Daisy firmware, or MIDI behavior.

## Legacy disposition

The existing `pkg/` and `test/` Go implementation is retained under `legacy/go-kata/` with its original README context. It is historical/reference material and is not linked, imported, or tested as part of the production realtime build. Known correctness concerns remain documented rather than silently repaired during bootstrap.

## New repository boundary

The new tree reserves these responsibilities:

```text
core/                 host-independent realtime contracts (issue #3+)
effects/              portable effects built on core contracts
faust/                authoritative Faust sources when adopted
hosts/native/          offline/native host and deterministic harness
hosts/juce/            optional JUCE adapter; not required by core
targets/daisy/         embedded adapter/firmware; not required by core
protocols/             bounded external configuration/evidence contracts
test/                  native vectors, golden tests, realtime benchmarks
docs/adr/              architecture decisions
legacy/go-kata/        preserved historical implementation
```

Only directories needed by the current bootstrap need files immediately. Empty future directories should not be committed merely to realize the diagram.

## Native build baseline

Use CMake with C++20 and Ninja in the reproducible shell. Define a linkable `dsp_core` interface target with no production behavior yet. A native smoke-test executable proves includes, linkage, and CTest wiring without inventing the processor API early.

The baseline commands are:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Reproducible development environment

Use a Nix flake pinned through `flake.lock`, following current Micrantha practice and the `nixos-26.05` nixpkgs channel. The development shell includes CMake, Ninja, Clang/LLVM tooling, Git, and standard build utilities. `nix flake check` performs a native configure/build/test derivation so CI and local verification share the same quality surface.

The core build must not require Faust, JUCE, Daisy SDKs, audio hardware, or the Go toolchain.

## CI

GitHub Actions runs the flake check on supported Linux runners. CI correctness must not depend on uploaded workflow artifacts. The test evidence is the command exit status/log itself.

## Realtime constraints

Bootstrap documentation establishes constraints that later issues must preserve:

- no dynamic allocation in the audio callback;
- no filesystem or network I/O in the audio callback;
- no blocking locks in the audio callback;
- bounded per-block work;
- explicit sample-rate and block-size contracts;
- deterministic realtime-safe control handoff;
- host/framework types stay outside core DSP interfaces.

Issue #2 documents these rules but does not yet create the callback API.

## Testing strategy

The bootstrap has no DSP behavior to unit test. Test-first work therefore targets repository/build behavior: first introduce a native smoke test that cannot build before CMake targets exist, observe the expected failure, then add the minimum build structure to make it pass. Configuration-only files are verified through configure/build/CTest and `nix flake check`.

## Success criteria

- legacy Go files are preserved under `legacy/go-kata/` and excluded from the new build;
- a fresh Nix shell can configure, build, and test the native C++ target;
- the native build is independent of future frameworks/hardware SDKs;
- CI invokes the same reproducible flake check;
- architecture and realtime constraints are visible from the root documentation;
- no production DSP API is invented before issue #3.
