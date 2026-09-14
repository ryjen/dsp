# dsp

Realtime guitar DSP, pedal firmware, realtime MIDI/control, and bounded performance-analysis experiments.

## Repository role

`ryjen/dsp` is the realtime execution plane. `ryjen/guitar-practice-system` remains the control/orchestration plane for `guitarctl`, Score/Song IR, practice logic, backing/groove generation, and non-realtime MIDI composition.

The repositories integrate only through explicit, versioned contracts; neither should depend on the other's source tree.

## Architecture

- `core/` — host-independent realtime contracts and fixed-state control primitives
- `effects/` — portable effects built on core contracts
- `hosts/native/` — deterministic offline rendering, fixtures, comparison, and timing evidence
- `hosts/` — future optional desktop host adapters
- `targets/` — embedded hardware adapters such as Daisy
- `protocols/` — bounded external configuration/evidence contracts
- `legacy/go-kata/` — preserved historical Go DSP exercises

Core DSP does not depend on JUCE, Daisy SDK types, filesystem/network APIs, or MIDI packet formats.

## Realtime contract

The core contract is defined by `ProcessSpec`, non-owning planar `AudioBlock`, and `Processor`. Preparation happens outside the audio callback; `reset()` and `process()` are bounded `noexcept` operations.

`AtomicFloat` provides lock-free scalar target publication and `LinearSmoother` provides fixed-state sample ramps. Concrete effects own their parameter sets rather than using a dynamic registry.

See `docs/adr/0001-realtime-processor-contract.md` for the rationale and extension rules.

## Faust effect kernels

Faust is adopted selectively as a build-time source generator for suitable DSP kernels. Public processor/control/host contracts remain native C++, and ordinary native builds consume committed generated C++ without requiring Faust.

The first portable effect is `TremoloProcessor`, generated from `effects/tremolo/faust/tremolo.dsp` and wrapped behind the same `Processor` contract as every other effect. Regenerate its committed kernel with:

```sh
nix develop --command tools/generate-faust.sh
```

`nix flake check` independently regenerates and compares the artifact through `checks.faust-generated`. Generated files under `effects/*/generated/` are never edited by hand. See `docs/adr/0002-selective-faust-adoption.md`.

## Native evidence harness

`dsp_native` can render deterministic vectors through the same `Processor::process()` boundary future hosts will call. It also provides fixture generation, absolute/relative sample comparison, and callback-budget reporting without requiring audio hardware.

Listening tests are not a correctness gate; deterministic vectors and allocation guards are.

## Build and test

```sh
nix develop
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Or run the pinned repository gate:

```sh
nix flake check --no-write-lock-file --print-build-logs
```

Issue #5 is the next DSP slice: implement bounded realtime delay with tempo-synchronized control.
