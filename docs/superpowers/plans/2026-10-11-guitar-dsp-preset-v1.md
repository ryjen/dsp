# guitar-dsp preset v1 implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the frozen `guitar-dsp.preset` v1 contract independently in `ryjen/dsp` and `ryjen/guitar-practice-system`, proving byte-identical canonical fixtures and bounded application to the existing delay processor.

**Architecture:** DSP owns a fixed-size C++ value/parser/serializer/binding with no generic JSON dependency. Guitar owns a pure Python validator/canonicalizer plus native `guitarctl` validate/canonicalize handlers. The repositories share only the frozen protocol bytes and issue/spec references.

**Tech Stack:** C++20/CMake/Nix in `ryjen/dsp`; Python 3.12 standard library and existing `guitarctl` architecture in `ryjen/guitar-practice-system`.

**Spec:** `docs/superpowers/specs/2026-10-11-guitar-dsp-preset-v1-design.md`

## Global Constraints

- Schema is exactly `guitar-dsp.preset`, version integer `1`.
- Canonical protocol bytes are at most 256 bytes and contain no terminal newline.
- v1 fields and parameter IDs are fixed; unknown/missing/duplicate/non-canonical input fails closed.
- Scaled integers: tempo 20..300 BPM; feedback 0..950; free delay 1..2000 ms; mix 0..1000.
- Subdivision enum: free, quarter, dotted-eighth, eighth, eighth-triplet, sixteenth.
- Preset slug is 1..32 chars and matches `^[a-z0-9]+(?:-[a-z0-9]+)*$`.
- No Score IR, practice-plan, MIDI packet, Daisy, filesystem path, shell, transport framing, or generic parameter registry in the protocol core.
- Canonical fixture bytes must be identical in both repositories.
- Apply failure must not mutate `DelayProcessor`.

## Review Focus

- 257-byte input must return `too_large` without reading beyond the bound.
- Semantically valid but reordered/whitespace JSON must return `non_canonical` on DSP while guitarctl canonicalizes it.
- Out-of-range integer values and invalid preset/subdivision strings must fail before serialization/application.
- Exact maximum-sized valid preset slug must round-trip without truncation or missing NUL safety in C++ storage.
- Applying a validated preset must set all five delay controls while leaving the opaque preset label uninterpreted.

---

### Task 1: DSP fixed value model and canonical serializer

**Repository:** `ryjen/dsp` branch `issue-7-guitarctl-dsp-protocol`

**Files:**
- Create: `protocols/include/dsp/protocols/guitar_dsp_preset_v1.hpp`
- Create: `protocols/src/guitar_dsp_preset_v1.cpp`
- Create: `protocols/fixtures/jazz-blues-delay-v1.json`
- Test: `test/protocols/guitar_dsp_preset_v1_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `ProtocolError`, `DelayPresetV1`, `SerializeResult`.
- Produces `serialize_preset_v1(const DelayPresetV1&, std::span<char>) noexcept`.
- Task 2 adds parsing to the same focused protocol module.

- [ ] **Step 1: Write failing serializer/value tests**

Pin the exact jazz-blues bytes from the spec, max 32-char preset slug, each scalar bound, subdivision spellings, insufficient output buffer, and zero dynamic allocation across 10,000 serializations.

- [ ] **Step 2: Run RED**

Run: `nix flake check --no-write-lock-file --print-build-logs`.

Expected: protocol target/header missing.

- [ ] **Step 3: Implement minimal fixed-size value and serializer**

Use `std::array<char, 33>` plus explicit length for preset identity. Write into caller-provided `std::span<char>`; no string allocation or generic JSON library.

- [ ] **Step 4: Run GREEN**

Run the full flake gate; exact fixture equality and existing tests must pass.

- [ ] **Step 5: Commit**

Commit message: `feat: add canonical guitar-dsp preset v1 serializer`.

### Task 2: DSP bounded canonical parser

**Repository:** `ryjen/dsp`

**Files:**
- Modify: `protocols/include/dsp/protocols/guitar_dsp_preset_v1.hpp`
- Modify: `protocols/src/guitar_dsp_preset_v1.cpp`
- Modify: `test/protocols/guitar_dsp_preset_v1_test.cpp`

**Interfaces:**
- Produces `ParseResult parse_preset_v1(std::string_view) noexcept`.
- Parser accepts only canonical v1 bytes and returns the spec's stable `ProtocolError` enum.

- [ ] **Step 1: Add failing parser tests**

Cover exact fixture round-trip, 257 bytes, whitespace/reordered keys, wrong schema/version, invalid slug, every numeric lower/upper bound plus one outside each, invalid subdivision, truncated input, and allocation-free 10,000 parses.

- [ ] **Step 2: Run RED**

Expected: parse interface absent.

- [ ] **Step 3: Implement bounded canonical parser**

Parse the frozen literal/key sequence directly from `std::string_view`. Use small integer/string token helpers; reject any unconsumed byte. Do not construct a DOM.

- [ ] **Step 4: Run GREEN**

Run full flake checks.

- [ ] **Step 5: Commit**

Commit message: `feat: parse bounded guitar-dsp preset v1`.

### Task 3: DSP delay application and protocol documentation

**Repository:** `ryjen/dsp`

**Files:**
- Create: `protocols/include/dsp/protocols/delay_preset_binding.hpp`
- Create: `protocols/src/delay_preset_binding.cpp`
- Test: `test/protocols/delay_preset_binding_test.cpp`
- Create: `docs/adr/0005-guitar-dsp-preset-v1.md`
- Modify: `README.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `apply_delay_preset_v1(const DelayPresetV1&, dsp::effects::DelayProcessor&) noexcept`.

- [ ] **Step 1: Write failing binding test**

Apply the canonical fixture and assert tempo=92, delay=375 ms, feedback=0.320, mix=0.500, subdivision=dotted-eighth; assert the preset label has no lookup effect.

- [ ] **Step 2: Run RED**

Expected: binding symbol missing.

- [ ] **Step 3: Implement direct binding**

Translate scaled integers to existing setters only. Keep protocol parsing separate from effect mutation.

- [ ] **Step 4: Document frozen v1 and run full exact-head gates**

Run `nix flake check --no-write-lock-file --print-build-logs` plus `git diff --check` and protocol/core target-boundary scans.

- [ ] **Step 5: Commit and open DSP PR**

Do not close #7 until guitar issue #152 proves independent conformance.

### Task 4: Guitar pure domain contract and canonical fixture

**Repository:** `ryjen/guitar-practice-system` branch `issue-152-dsp-preset-v1`

**Files:**
- Create: `guitar_practice/domain/dsp_preset.py`
- Create: `examples/dsp-presets/jazz-blues-delay-v1.json`
- Test: `tests/test_dsp_preset.py`
- Modify: `guitar_practice/domain/README.md`

**Interfaces:**
- Produces `DspPresetError(ValueError)`.
- Produces `validate(document: Mapping[str, Any]) -> None`.
- Produces `dumps(document: Mapping[str, Any]) -> str` and `loads(text: str) -> dict[str, Any]`.
- Canonical `dumps` returns the exact wire string without newline.

- [ ] **Step 1: Write failing domain tests**

Cover exact fixture bytes, normal JSON input with reordered keys/whitespace canonicalizing to the fixture, all v1 bounds, booleans rejected as integers, unknown/missing fields/IDs, version/schema errors, preset/subdivision rules, and deterministic round-trip.

- [ ] **Step 2: Run RED**

Run: `python -m unittest tests.test_dsp_preset -v`.

Expected: module missing.

- [ ] **Step 3: Implement standard-library validator/canonicalizer**

Use `json.loads` only at the guitar user boundary. Build the canonical string explicitly in frozen key order rather than relying on dict order as protocol semantics.

- [ ] **Step 4: Run GREEN and repository validation**

Run targeted unit test, then `python scripts/validate_repo.py`.

- [ ] **Step 5: Commit**

Commit message: `feat: add guitar-dsp preset v1 domain contract`.

### Task 5: Native guitarctl validate/canonicalize commands

**Repository:** `ryjen/guitar-practice-system`

**Files:**
- Create: `guitar_practice/interfaces/cli/dsp_handlers.py`
- Modify: `guitar_practice/interfaces/cli/commands.py`
- Modify: `guitar_practice/interfaces/cli/handler_registry.py`
- Test: `tests/test_native_dsp_cli.py`
- Modify: `docs/cli.md`
- Modify: `README.md`

**Interfaces:**
- Registers `dsp-preset-validate` for `guitarctl dsp preset validate <document>`.
- Registers `dsp-preset-canonicalize` for `guitarctl dsp preset canonicalize <document>`.
- Uses existing `CliContext.workspace/stdout/stderr`; no subprocess or device transport.

- [ ] **Step 1: Write failing CLI tests**

Validate success output is structured JSON; canonicalize stdout is exact canonical protocol bytes plus the CLI's terminal newline; invalid input returns the existing data/usage error convention on stderr; command registry remains unique.

- [ ] **Step 2: Run RED**

Expected: command paths absent.

- [ ] **Step 3: Implement focused native handler module**

Read one explicit workspace-relative document through the existing bounded CLI/file adapter pattern. Delegate all semantics to `domain.dsp_preset`.

- [ ] **Step 4: Run full guitar validation**

Run `python scripts/validate_repo.py` and packaged CLI smoke commands.

- [ ] **Step 5: Commit and open guitar PR**

Reference `ryjen/dsp#7` and the exact DSP fixture bytes.

### Task 6: Cross-repo conformance and integration

**Repositories:** both

- [ ] **Step 1: Compare canonical fixtures byte-for-byte**

Read both committed fixture files and assert exact equality externally without introducing a source dependency.

- [ ] **Step 2: Require exact-head hosted CI on both PRs**

DSP: Nix/native/strict/sanitizer/firmware gates. Guitar: repository validation workflow.

- [ ] **Step 3: Whole-change review**

Reject any new Score IR/practice-domain coupling, generic parameter registry, hardware transport, arbitrary parser allocation, or mismatch in version/bounds/error semantics.

- [ ] **Step 4: Merge DSP and guitar PRs with expected-head protection**

Order is not semantically significant because neither repository imports the other; prefer DSP first so the consumer contract is landed before producer docs point at it.

- [ ] **Step 5: Update issues**

Close `ryjen/dsp#7` and `ryjen/guitar-practice-system#152` only after both post-merge CI runs are green and fixture bytes remain identical. Update epic #1 and guitar #94 relationship notes without making #152 part of #94's migration closure criteria.
