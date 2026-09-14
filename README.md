# dsp

Realtime guitar DSP, pedal firmware, realtime MIDI/control, and bounded performance-analysis experiments.

## Repository role

`ryjen/dsp` is the realtime execution plane. `ryjen/guitar-practice-system` remains the control/orchestration plane for `guitarctl`, Score/Song IR, practice logic, backing/groove generation, and non-realtime MIDI composition.

The repositories integrate only through explicit, versioned contracts; neither should depend on the other's source tree.

## Architecture direction

- `core/` — host-independent realtime contracts (introduced in issue #3)
- `effects/` — portable effects built on core contracts
- `hosts/` — native/offline and optional desktop host adapters
- `targets/` — embedded hardware adapters such as Daisy
- `protocols/` — bounded external configuration/evidence contracts
- `legacy/go-kata/` — preserved historical Go DSP exercises

Core DSP must not depend on JUCE, Daisy SDK types, filesystem/network APIs, or MIDI packet formats.

## Realtime baseline

Future audio callbacks must use bounded work with no dynamic allocation, filesystem/network I/O, or blocking locks. Sample-rate/block-size assumptions and control handoff must be explicit and deterministic.

## Bootstrap build

The current bootstrap intentionally defines no production DSP processor API. Issue #2 only establishes the build/test/tooling boundary; issue #3 owns the processor contract and offline harness.

See `docs/superpowers/specs/2026-09-14-realtime-dsp-bootstrap-design.md` for the bootstrap design.
