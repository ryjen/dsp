# ADR 0005: Frozen guitar-DSP preset v1 boundary

- **Status:** Accepted
- **Date:** 2026-10-11
- **Issue:** #7
- **Producer:** `ryjen/guitar-practice-system#152` with wire-conformance follow-up #158

## Context

The practice/control plane needs to configure the proven native delay effect without
sharing source code, Score IR, practice-plan state, transport details, or hardware
types with the realtime DSP repository.

The constrained target also needs bounded parsing: no generic JSON DOM, no heap
allocation, deterministic version handling, and a fixed input size.

## Decision

DSP accepts a frozen canonical `guitar-dsp.preset` version 1 document. The protocol
module owns only parsing, validation, canonical serialization, and the fixed-size
`DelayPresetV1` value. A separate binding adapter translates that validated value
onto `DelayProcessor`.

The v1 wire representation is:

- maximum 256 bytes;
- no terminal newline or insignificant whitespace;
- preset identity is an opaque 1..32 ASCII slug;
- integer BPM 20..300;
- feedback/mix are integer milli-units;
- free delay is integer milliseconds;
- subdivision is one of `free`, `quarter`, `dotted-eighth`, `eighth`,
  `eighth-triplet`, or `sixteenth`;
- unknown, missing, reordered, whitespace-expanded, newer-version, or otherwise
  non-canonical input fails closed in the DSP parser.

The DSP parser accepts canonical wire bytes only. Human-friendly JSON
canonicalization belongs to the producer/control plane.

## Application semantics

`apply_delay_preset_v1` validates the complete value before mutating the processor,
then applies:

- `free_ms` to the stored free delay time;
- `feedback_milli / 1000` to feedback;
- `mix_milli / 1000` to wet/dry mix;
- `tempo_bpm` to normalized tempo;
- the explicit subdivision mode.

The preset label is identity only and never selects hidden DSP state.

## Dependency direction

```text
guitar-practice-system
  authors / validates / canonicalizes
              |
              | canonical v1 bytes
              v
dsp::protocols
  bounded parse / validate
              |
              v
delay preset binding
              |
              v
DelayProcessor
```

Neither repository imports the other's source tree. The protocol core has no Daisy,
MIDI-packet, filesystem, shell, network, or transport-framing dependency.

## Consequences

A future incompatible field, bound, parameter set, or canonicalization rule requires
a new explicit protocol version. Transport adapters may carry v1 bytes later, but
transport is not part of this ADR or issue #7.
