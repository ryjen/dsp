# guitar-dsp preset v1 protocol design

- **Status:** Proposed
- **Date:** 2026-10-11
- **Issue:** #7
- **Producer:** ryjen/guitar-practice-system#152
- **Depends on:** #3, #5, #6

## Intent

Define the smallest versioned contract by which the guitar practice/control plane can configure the concrete tempo-synchronized delay feature now proven in the DSP runtime.

The protocol is an anti-corruption boundary, not a shared domain model. `guitar-practice-system` may derive, author, validate, and canonicalize documents; `dsp` independently parses and applies the supported realtime values. Neither repository imports the other's source tree.

v1 deliberately does not model Score IR, progressions, keys, modes, practice plans, hardware pins, MIDI packets, transport framing, plugin registries, or arbitrary effect graphs.

## Contract shape

The canonical v1 document is:

```json
{"schema":"guitar-dsp.preset","version":1,"preset":"jazz-blues","tempo_bpm":92,"parameters":{"delay.feedback_milli":320,"delay.free_ms":375,"delay.mix_milli":500,"delay.subdivision":"dotted-eighth"}}
```

Top-level fields are exactly:

1. `schema`
2. `version`
3. `preset`
4. `tempo_bpm`
5. `parameters`

The parameter object contains exactly these v1 keys in canonical lexicographic order:

1. `delay.feedback_milli`
2. `delay.free_ms`
3. `delay.mix_milli`
4. `delay.subdivision`

All four delay parameters are required. `delay.free_ms` remains meaningful state even when a synchronized subdivision is selected, matching the existing `DelayProcessor` behavior: switching back to `free` restores the configured free-time value.

## Why scaled integers

v1 avoids cross-language floating-point text canonicalization.

- `tempo_bpm`: integer BPM, 20..300
- `delay.feedback_milli`: integer 0..950, applied as value / 1000
- `delay.free_ms`: integer 1..2000
- `delay.mix_milli`: integer 0..1000, applied as value / 1000
- `delay.subdivision`: one of:
  - `free`
  - `quarter`
  - `dotted-eighth`
  - `eighth`
  - `eighth-triplet`
  - `sixteenth`

This preserves the useful control resolution already present in the effect while making byte-identical Python/C++ fixtures straightforward.

## Preset identity

`preset` is an opaque identity label only. It does not select hidden firmware state and must not alter interpretation of `parameters`.

It is:

- 1..32 ASCII characters;
- lower-case letters and digits separated by single hyphens;
- equivalent to `^[a-z0-9]+(?:-[a-z0-9]+)*$`.

The canonical `jazz-blues` fixture is evidence, not a built-in DSP preset registry.

## Canonical wire representation

The protocol's wire representation is canonical JSON with these rules:

- UTF-8, restricted by v1 values to ASCII;
- no BOM;
- no leading or trailing whitespace;
- no terminal newline in the protocol byte sequence;
- exact top-level and parameter key order defined above;
- no insignificant whitespace;
- decimal integers use the shortest base-10 representation with no leading zero except `0`;
- strings use only the allowed preset/subdivision character sets, so canonical v1 needs no escape sequences;
- maximum canonical document size: 256 bytes.

The example above is the canonical byte sequence for the canonical jazz-blues fixture.

### Producer behavior

`guitar-practice-system` may accept normal parsed JSON documents at its user-facing boundary. Its domain validator checks semantics and its canonical serializer emits the exact wire representation.

`guitarctl dsp preset canonicalize <document>` therefore converts a human-edited source document into the bytes suitable for a DSP transport adapter.

### DSP behavior

The DSP-side parser consumes canonical wire bytes only.

It is not a general-purpose JSON parser. Semantically equivalent but non-canonical JSON—for example reordered keys or added whitespace—is rejected as `non_canonical`.

This keeps firmware parsing bounded, allocation-free, auditable, and independent of a generic JSON dependency. A future host adapter may choose to canonicalize richer input before invoking the core protocol parser.

Raw input is capped at 256 bytes before parsing.

## Versioning

The envelope is exact:

- `schema` must equal `guitar-dsp.preset`;
- `version` must equal integer `1`.

A v1 consumer rejects any other version with `unsupported_version`. It never guesses compatibility and never silently downgrades.

Backward compatibility is producer-driven: future guitarctl versions that target a v1 device retain the ability to emit this frozen v1 representation. A future DSP implementation may support multiple explicit protocol versions side by side.

Changing v1 field meaning, bounds, required keys, or canonical byte rules is not permitted after acceptance. New semantics require a new version.

## Unknown-field behavior

v1 is fail-closed:

- unknown top-level fields: reject;
- missing fields: reject;
- duplicate fields: reject;
- unknown parameter IDs: reject;
- missing v1 parameter IDs: reject;
- arrays/objects below `parameters`: reject;
- unknown subdivision: reject.

There is no extension bag in v1. This avoids silent configuration loss on constrained targets.

## Error model

The DSP protocol parser returns a bounded error enum rather than text assembled in firmware:

- `none`
- `too_large`
- `non_canonical`
- `wrong_schema`
- `unsupported_version`
- `invalid_preset`
- `invalid_tempo`
- `invalid_feedback`
- `invalid_free_ms`
- `invalid_mix`
- `invalid_subdivision`

Because the DSP parser accepts only canonical field layout, missing/unknown/duplicate/structural JSON cases collapse to `non_canonical` unless a more specific validated field error has already been reached.

Host/CLI adapters may render human-readable diagnostics from these stable codes.

## DSP value model

The parsed value is fixed-size and owns no heap state conceptually:

```cpp
struct DelayPresetV1 {
    std::array<char, 33> preset{};
    std::uint8_t preset_length{};
    std::uint16_t tempo_bpm{120};
    std::uint16_t feedback_milli{};
    std::uint16_t free_ms{250};
    std::uint16_t mix_milli{500};
    DelaySubdivision subdivision{DelaySubdivision::free};
};
```

Exact integer widths may be adjusted if compilation proves a clearer representation, but the value remains trivially bounded and transport/hardware independent.

Parsing consumes `std::string_view` and returns a value/error result without allocating. Serialization writes into a caller-provided character span and reports bytes written/error rather than allocating a string.

## Applying a preset

Protocol parsing and effect application are separate responsibilities.

A small application/binding function maps a validated `DelayPresetV1` into the existing `DelayProcessor`:

```text
tempo_bpm              -> set_tempo_bpm
feedback_milli / 1000 -> set_feedback
free_ms                -> set_delay_ms
mix_milli / 1000      -> set_mix
subdivision            -> set_subdivision
```

`preset` is not interpreted.

No MIDI packets, Daisy types, Score IR, filesystem paths, or device transport concerns appear in this binding.

A parse failure performs no effect mutation.

## Transport and acknowledgement

v1 defines a configuration document, not a live transfer framing protocol.

USB serial, SysEx, file transfer, capability negotiation, acknowledgements, retries, and authentication are outside this slice. A future transport adapter may carry the exact canonical v1 bytes and define its own bounded request/ack frame without changing the preset document.

MIDI Start/Stop/Continue also remain realtime transport/control concerns from #6; they are not preset configuration fields.

## Guitar-side ownership

`guitar-practice-system#152` owns:

- a pure standard-library Python validator/value contract;
- canonical byte serialization;
- canonical fixture generation/checking;
- `guitarctl dsp preset validate`;
- `guitarctl dsp preset canonicalize`;
- user-facing diagnostics.

It must not import the C++ repository or infer hidden DSP values from Score IR/practice state.

## DSP-side ownership

`ryjen/dsp#7` owns:

- fixed-size v1 value/error types;
- bounded canonical parser;
- bounded canonical serializer;
- application to the existing delay processor;
- C++ canonical fixture tests;
- protocol documentation.

It must not import or mirror guitar-practice domain objects.

## Shared conformance fixture

Both repositories keep a local byte-identical canonical fixture for:

- preset: `jazz-blues`
- tempo: `92`
- feedback: `320`
- free delay: `375`
- mix: `500`
- subdivision: `dotted-eighth`

This duplication is intentional protocol conformance evidence, not a source-code dependency.

Each repository asserts that its canonical serializer produces exactly that fixture. Cross-repo issue links record the shared contract version.

## Security and resource bounds

v1 contains:

- no executable text;
- no shell fragments;
- no filesystem paths;
- no URLs/network destinations;
- no arbitrary nesting;
- no unbounded arrays/maps;
- no arbitrary parameter names;
- no floating-point textual edge cases.

The DSP parser performs work proportional to at most 256 input bytes and cannot allocate based on input length.

## Deferred work

- device capability discovery;
- transport framing and acknowledgement;
- live preset transfer;
- persistent preset storage;
- multiple effects/effect graphs;
- generic parameter registries;
- non-integer BPM;
- automated mapping from practice/Score IR concepts to DSP settings;
- authentication/authorization for a mutable device transport.

## Acceptance mapping

#7 is complete when:

- v1 schema/version and canonical bytes are frozen and documented;
- DSP parser/serializer safely reject malformed, oversized, out-of-range, unknown-version, and non-canonical input;
- canonical jazz-blues fixture round-trips byte-for-byte;
- validated v1 applies the expected tempo/delay values to the existing portable `DelayProcessor`;
- no guitar-practice domain model appears in the DSP implementation;
- `guitar-practice-system#152` independently validates/canonicalizes the same fixture;
- cross-repo ownership is documented.

Live transport is explicitly not required to close v1.
