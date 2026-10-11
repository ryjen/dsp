# ADR-0005: guitar-DSP preset v1

- **Status:** Accepted
- **Date:** 2026-10-11
- **Issue:** #7
- **Producer:** ryjen/guitar-practice-system#152 / #158
- **Related:** ADR-0003, ADR-0004

## Context

The practice/control plane and realtime DSP repository need one explicit integration contract without sharing source trees or copying practice-domain models into firmware.

The first concrete consumer is the tempo-synchronized `DelayProcessor` already proven by #5/#6. A general plugin graph, dynamic parameter registry, or Score IR projection would add abstractions not required by that use case and would complicate bounded embedded parsing.

## Decision

Freeze `guitar-dsp.preset` version 1 as canonical JSON with exactly:

- schema and integer version;
- a 1–32 character lowercase slug used only as identity;
- integer tempo from 20–300 BPM;
- `delay.feedback_milli` from 0–950;
- `delay.free_ms` from 1–2000;
- `delay.mix_milli` from 0–1000;
- one fixed delay subdivision enum.

Scaled integer controls avoid cross-language floating-point text canonicalization.

The DSP parser accepts only canonical wire bytes, capped at 256 bytes. It uses a fixed-size value object, direct literal/token parsing, caller-provided serialization storage, and no generic JSON library or input-sized allocation.

The guitar producer may accept ordinary human-authored JSON, but must validate and emit the frozen canonical representation before any future transport adapter sends it.

## Canonical fixture

The v1 conformance fixture is:

```json
{"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}}
```

It is 199 bytes with no terminal newline. The fixture is intentionally duplicated in both repositories as protocol conformance evidence. The files are byte-identical rather than linked or vendored.

## Versioning and failure behavior

A v1 consumer rejects:

- schemas other than `guitar-dsp.preset`;
- versions other than integer `1`;
- unknown/missing/reordered wire fields;
- non-canonical whitespace or number spelling;
- invalid preset IDs;
- out-of-range values;
- unknown subdivisions;
- input larger than 256 bytes.

The parser never guesses compatibility or silently downgrades. Future incompatible semantics require a new explicit version.

## Application boundary

Parsing and effect mutation are separate.

`apply_delay_preset_v1` maps only the validated four delay controls plus tempo into the existing `DelayProcessor`. The preset slug is opaque and does not select hidden state.

No MIDI packet, Daisy, filesystem, Score IR, progression, practice-plan, key/mode, shell, or network concept appears in the protocol value or binding.

## Transport

v1 defines a configuration document only. It does not define USB/serial/SysEx framing, acknowledgements, retries, authentication, capability negotiation, or persistence.

A future transport may carry these exact bytes without changing v1 semantics.

## Consequences

Benefits:

- deterministic cross-language bytes;
- bounded allocation-free embedded parsing;
- explicit ownership between guitar orchestration and DSP execution;
- no source dependency between repositories;
- concrete protocol shaped by an implemented effect rather than a speculative plugin system.

Costs:

- v1 is intentionally delay-specific;
- human-authored non-canonical JSON must be canonicalized before reaching the DSP parser;
- adding effects or richer tempo representation requires a future version rather than silent extension.
