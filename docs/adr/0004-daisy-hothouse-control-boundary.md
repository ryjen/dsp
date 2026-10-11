# ADR-0004: Daisy Hothouse control boundary

- **Status:** Accepted
- **Date:** 2026-10-11
- **Issue:** #6
- **Related:** ADR-0001, ADR-0003

## Context

The realtime core and native effects are intentionally independent of hosts and hardware. The first embedded target must prove that the existing `DelayProcessor` can be composed into pedal firmware without moving libDaisy, Hothouse pin knowledge, raw MIDI packet formats, or hardware-control policy into `core/` or `effects/`.

The first target also exposes two embedded constraints that host-only tests do not: fixed callback deadlines and bounded memory regions. At 48 kHz with a two-second stereo maximum delay, the delay history requires roughly 768 KiB and therefore cannot rely on the Daisy internal SRAM heap.

## Decision

Use Daisy Seed/Seed3 on a stereo Hothouse carrier as the first embedded target, implemented as a thin adapter under `targets/daisy/`.

The dependency direction is:

```text
portable Processor / DelayProcessor
              ^
              |
fixed control state + tempo policy
              ^
              |
Daisy/Hothouse adapters and composition root
```

The firmware target uses the same `DelayProcessor` implementation as native tests. Daisy-specific source code may include portable headers; portable code may not include Daisy, Hothouse, or raw MIDI packet types.

## Control publication

Use fixed latest-value state rather than a dynamic event bus or parameter registry.

The single-writer control loop validates physical controls and decoded MIDI, updates deterministic tempo/control policy, and publishes bounded scalar state. The audio/runtime side snapshots control state once per block.

Continuous controls are latest-value-wins values. Debounced momentary controls are converted to final state before publication. No unbounded queue or callback-time parsing is introduced.

## Snapshot consistency model

`PedalControlState` intentionally provides field-wise latest-value publication rather than aggregate transactional snapshots. Each scalar is independently bounded and semantically valid with any contemporaneous value of the other controls; there is no cross-field invariant between primary, secondary, tempo, mode, bypass, and transport.

This avoids locks, allocation, and retry loops in the realtime handoff. Code that introduces future state with a true cross-field invariant must use a separate coherency mechanism rather than assuming this state object provides transactional generations.

## MIDI and tempo ownership

The first physical transport is USB MIDI.

Supported MIDI behavior is deliberately small:

- Timing Clock at 24 PPQN;
- Start, Stop, and Continue;
- channel-1 CC20 for the primary/free-delay control;
- channel-1 CC21 for feedback.

Unsupported channels/messages, including Program Change, are ignored in this slice.

Valid running MIDI clock has priority over tap tempo. Stop or a monotonic-time clock timeout releases MIDI authority. Tap tempo is accepted only while MIDI is not authoritative. The default tempo is 120 BPM until a valid source establishes another bounded 20–300 BPM value.

Raw libDaisy `MidiEvent` values are translated at the target boundary into project-owned bounded MIDI values before the portable translation/policy layer sees them.

SystemRealTime events take a timing-preserving path: libDaisy's realtime receive callback records the 32-bit microsecond arrival tick and enqueues the project-owned message in a fixed 64-entry SPSC inbox. The callback never mutates tempo or processor state. The single-writer main loop drains the inbox and performs policy mutation. A full inbox drops the newest message rather than blocking or allocating.

Daisy's microsecond counter wraps at 32 bits. Tempo/debounce policy therefore uses modulo-32 unsigned elapsed arithmetic with a half-range validity bound; all accepted control/timing windows are only milliseconds or seconds. Timeout clears stale MIDI interval history while retaining the last audible tempo so resumed clock must establish fresh timing before reclaiming authority.

## Physical controls

The reference binding is intentionally concrete:

- Pot 1 -> exponential 1–2000 ms free delay;
- Pot 2 -> linear 0–0.95 feedback;
- Toggle 1 -> free / quarter / dotted-eighth subdivision;
- Footswitch 1 -> bypass;
- Footswitch 2 -> tap tempo.

Logical mapping/debounce policy is tested without hardware. Hothouse pin facts remain target-local.

## Bypass

Bypass is a runtime/target concern, not a `DelayProcessor` responsibility.

The runtime prepares dry scratch sized to the process specification and performs a 5 ms dry/processed crossfade. The processor continues running while bypassed so state and delay tails remain continuous when the effect is re-engaged.

## Delay memory ownership

`DelayProcessor` supports both host-owned storage and caller-owned external storage.

Desktop/native preparation may allocate its bounded delay storage outside the callback. Embedded firmware provides caller-owned storage in Daisy SDRAM, preserving the same processor implementation while avoiding an impossible internal-SRAM heap requirement. External-storage preparation validates a candidate span before committing the binding, so a failed prepare leaves the processor able to use its previous/owned storage path.

The Hothouse firmware reserves 192,004 float samples in SDRAM for the 48 kHz / stereo / two-second delay configuration. The linker evidence from the hosted cross-build reports approximately 768 KiB of SDRAM use and low internal SRAM use.

## Build and dependency boundary

The repository flake pins:

- the Arm GNU Embedded toolchain through nixpkgs;
- libDaisy at commit `facb66c76b5482918741695f4268b0185e474644`;
- the fixed-output libDaisy source hash;
- libDaisy submodules.

Hosted CI cross-builds an ARM `hothouse_delay.elf` and `.bin` rather than checking only for source-file presence.

The Cleveland Music Co. `HothouseExamples` firmware repository is GPL-3.0 and is not copied, linked, vendored, or used as a build dependency. Hardware pin facts were derived from the separately published CC-BY-SA-4.0 open-hardware schematic and are kept as factual target-local mappings.

## Realtime constraints

The firmware target is configured for 48 kHz / 48-frame stereo audio, giving a nominal 1 ms callback deadline.

The audio callback performs only bounded audio/runtime work plus CPU-load timing. The USB realtime callback only timestamps/translates and enqueues a bounded MIDI message; physical-control sampling, debounce, MIDI policy validation, and processor/control mutation stay in the main/control loop. Callback allocation, filesystem/network I/O, logging, blocking locks, and unbounded parsing remain prohibited.

`CpuLoadMeter` is initialized for the configured block/sample rate, but measured hardware headroom must be recorded outside the callback on real hardware before #6 is complete.

## Denormal policy

Cross-compilation does not establish runtime subnormal/denormal behavior. The software slice therefore does not claim a completed policy based on inference alone.

Hardware validation must either confirm the runtime flush-to-zero behavior or explicitly configure a supported policy, then exercise a long feedback decay while observing callback load and output finiteness. A portable suppression mechanism should be added only if hardware evidence requires it.

## Evidence

Software evidence currently includes:

- deterministic native tests for control publication, MIDI/tempo policy, Hothouse logical control mapping, bypass/runtime behavior, delay external storage, and callback allocation bounds;
- strict warnings-as-errors and ASan/UBSan checks;
- target-boundary scanning that rejects Daisy/MIDI packet dependencies in `core/` or `effects/`;
- hosted ARM cross-compilation of the firmware;
- linker memory-region evidence showing the delay state in SDRAM.

Hardware-only claims remain explicitly pending in `targets/daisy/HARDWARE-VALIDATION.md`.

## Consequences

Benefits:

- one portable DSP implementation across native and embedded builds;
- explicit memory ownership suitable for constrained targets;
- testable control/tempo policy independent of hardware;
- transport and Hothouse details remain replaceable adapters;
- future #7 external control can target the same normalized boundary.

Costs:

- the first firmware target is intentionally Hothouse-specific at the composition edge;
- SDRAM becomes required for the reference two-second stereo delay configuration;
- actual callback headroom and denormal behavior cannot be closed in CI;
- Program Change/persistent presets remain deferred.
