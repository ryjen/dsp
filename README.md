# dsp

Realtime guitar DSP, pedal firmware, realtime MIDI/control, and bounded performance-analysis experiments.

## Repository role

`ryjen/dsp` is the realtime execution plane. `ryjen/guitar-practice-system` remains the control/orchestration plane for `guitarctl`, canonical Score IR, practice logic, backing/groove generation, and non-realtime MIDI composition.

The repositories integrate only through explicit, versioned contracts; neither should depend on the other's source tree. Score IR remains owned by `guitar-practice-system` and is not a DSP runtime model; DSP-facing contracts should expose only the normalized preset/control/evidence data required by the realtime boundary.

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

## Native stateful effects

`DelayProcessor` is the complementary native-C++ effect slice. It owns preallocated per-channel circular buffers, supports fractional reads, bounds feedback to 0.95, and smooths live delay-time, feedback, and wet/dry changes without callback allocation.

Delay time can be supplied directly in milliseconds or resolved from normalized BPM plus quarter, dotted-eighth, eighth, eighth-triplet, or sixteenth subdivisions. MIDI clock/packet decoding remains an adapter concern; the delay consumes only normalized effect controls.

See `docs/adr/0003-native-stateful-delay.md` for the state-ownership, interpolation, and transition decisions.

## Daisy / Hothouse target

Issue #6 adds a thin embedded adapter around the same portable `DelayProcessor` used by native tests. The target composes Hothouse physical controls, USB MIDI, deterministic tempo ownership, prepared bypass scratch, and libDaisy audio buffers without allowing Daisy or MIDI packet types into `core/` or `effects/`.

The first firmware target is configured for 48 kHz / 48-frame stereo audio. Its two-second stereo delay history is supplied as caller-owned SDRAM rather than consuming the Daisy internal SRAM heap. `nix flake check` cross-builds the firmware with a pinned Arm GNU Embedded toolchain and pinned libDaisy source with submodules.

Cross-compilation is software evidence only. Issue #6 remains open until a real Hothouse/Daisy board records flash/boot, control behavior, USB MIDI behavior, actual sample/block configuration, callback/CPU headroom, bypass behavior, and denormal-decay evidence. See `targets/daisy/HARDWARE-VALIDATION.md`.

## Guitar control-plane preset protocol

`protocols/` contains the independent DSP consumer for the frozen
`guitar-dsp.preset` v1 contract. The parser accepts only canonical bounded wire bytes
and allocates no memory during parse/serialization. A separate binding validates the
complete preset before applying normalized values to `DelayProcessor`; the opaque
preset name never selects hidden firmware state.

`ryjen/guitar-practice-system` owns authoring and human-JSON canonicalization.
Neither repository imports the other's source tree. See
`docs/adr/0005-guitar-dsp-preset-v1.md`.

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

Issue #6 has a software-complete Daisy/Hothouse prototype under review; real hardware validation remains the acceptance blocker. Issue #7 defines the independent bounded guitar-control-plane preset protocol and remains separate from physical transport/hardware validation.