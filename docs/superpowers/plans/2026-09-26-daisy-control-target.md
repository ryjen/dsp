# Daisy Pedal Target and Realtime Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a testable realtime control layer and Hothouse/Daisy firmware target that runs the existing `DelayProcessor` without leaking Daisy or raw MIDI concerns into portable DSP.

**Architecture:** A fixed latest-value control snapshot is published by a single writer and snapshotted once per audio block through lock-free scalar storage. USB MIDI, Hothouse controls, tap tempo, and bypass stay outside `core/` and `effects/`; the Daisy runtime adapts hardware buffers to the existing in-place `Processor` contract and crossfades against prepared dry scratch.

**Tech Stack:** C++20, CMake/Ninja, Nix, libDaisy 7.x pinned at commit `facb66c76b5482918741695f4268b0185e474644`, ARM GNU embedded toolchain, existing `dsp_core` and `dsp_delay`.

**Spec:** `docs/superpowers/specs/2026-09-26-daisy-control-target-design.md`

## Global Constraints

- Reference processor: existing `dsp::effects::DelayProcessor`; no Daisy-specific effect fork.
- First physical transport: USB MIDI only.
- MIDI Clock is 24 PPQN; channel-1 CC 20 controls primary/free delay and CC 21 controls feedback; other channels/messages including Program Change are ignored.
- Tempo range: 20–300 BPM; default 120 BPM.
- MIDI clock authority timeout: `max(250 ms, 6 * expected_clock_interval)`.
- Tap intervals: 200–3000 ms; two valid taps establish tempo; average at most four recent valid intervals.
- Pot 1: `delay_ms = exp(log(2000) * normalized)`; Pot 2: `feedback = 0.95 * normalized`.
- Toggle 1: free / quarter / dotted-eighth subdivision.
- Footswitch/toggle debounce: 20 ms; pot publication deadband: 0.002 normalized units.
- Prototype audio configuration: 48 kHz / 48 frames; 1 ms nominal callback deadline.
- Bypass uses 5 ms dry/processed crossfade with prepared dry scratch while `DelayProcessor` continues running.
- No allocation, filesystem/network I/O, logging, blocking locks, or unbounded parsing in the audio callback.
- `core/` and `effects/` may not include libDaisy/Daisy/Hothouse/MIDI packet types.
- `HothouseExamples` remains reference-only because it is GPL-3.0; implementation depends only on MIT libDaisy plus published hardware/pin data.
- #6 remains open until real hardware smoke/headroom/denormal evidence exists.
- Apply Clean Architecture and SOLID: keep policy/control logic independent of libDaisy, keep each unit single-purpose, depend on narrow abstractions/value types, and compose concrete adapters at the target boundary.
- Prefer composition and explicit Strategy/Adapter seams where behavior genuinely varies; do not introduce factories, registries, service locators, inheritance hierarchies, or generic frameworks without a concrete second implementation/use case.
- Keep source files focused: control publication, tempo arbitration, MIDI translation, Hothouse mapping, runtime/bypass, and firmware composition remain separate responsibilities.

## Design Quality Gate

Every task review must reject changes that:
- make `core/` or effect code depend on target details;
- combine parsing, policy, mapping, and I/O in one class;
- require mocks for ordinary domain behavior instead of dependency inversion/value inputs;
- add abstraction without a present consumer;
- hide ownership, lifetime, or realtime work behind global/service-locator state.

Patterns expected where they earn their keep: **Adapter** for libDaisy/MIDI hardware translation, **Strategy-by-composition** for effect/control binding if a second binding appears, and **State/value-object** modeling for tempo/control publication. Do not pattern-match for its own sake.

## Review Focus

- **Clock loss near 20 BPM:** six expected clock intervals must not release authority prematurely; Task 2 tests the slowest valid BPM and timeout boundary.
- **Noisy pots / bouncing switches:** sub-deadband ADC movement and <20 ms switch bounce must not republish/retrigger state; Task 3 pins both.
- **Rapid MIDI/control changes:** thousands of valid updates must stay bounded and allocation-free before the callback snapshot; Tasks 1 and 2 stress this.
- **Bypass transitions with delay tails:** bypass must preserve dry continuity without resetting the delay and must not allocate; Task 4 exercises repeated bypass cycles and tail continuity.
- **Malformed/unsupported MIDI:** wrong channel, unsupported status/CC, incomplete translated events, and non-monotonic timestamps must fail closed; Task 2 covers each class.

---

### Task 1: Fixed realtime control publication

**Files:**
- Create: `core/include/dsp/control/pedal_control.hpp`
- Create: `core/include/dsp/control/pedal_control_state.hpp`
- Test: `test/control/pedal_control_state_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `enum class PedalMode : std::uint8_t { free, quarter, dotted_eighth };`
- Produces `struct PedalControlSnapshot { float primary_normalized; float secondary_normalized; float tempo_bpm; PedalMode mode; bool bypassed; bool transport_running; };`
- Produces `class PedalControlState` with `publish(const PedalControlSnapshot&) noexcept` and `snapshot() const noexcept -> PedalControlSnapshot`.
- `snapshot()` is a bounded field-wise latest-value snapshot: every field is loaded once per call, but no transactional cross-field generation guarantee is required.
- Later tasks use this as the only portable state handoff into the audio/runtime layer.

- [ ] **Step 1: Write the failing state test**

Create `test/control/pedal_control_state_test.cpp` with assertions that:
- default snapshot is primary=0, secondary=0, tempo=120, mode=free, bypass=false, transport=false;
- `publish()` clamps primary/secondary to `[0,1]` and tempo to `[20,300]`;
- non-finite float inputs preserve the previous valid published value;
- invalid mode values normalize to `PedalMode::free`;
- 10,000 sequential publish/snapshot cycles preserve bounded values and allocate no memory after construction.

Use the existing global allocation-counter pattern from `test/core/realtime_allocation_test.cpp`.

- [ ] **Step 2: Run RED**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target dsp_pedal_control_state_test`

Expected: compile/configuration failure because the control headers/target do not exist.

- [ ] **Step 3: Implement the fixed lock-free state**

Implement `PedalControlState` with existing `AtomicFloat` for float fields and always-lock-free integer atomics for enum/bool fields. `publish()` validates each incoming field before release-store; `snapshot()` acquire-loads each field once and returns a value object.

Do not add an event queue or generic parameter IDs.

- [ ] **Step 4: Run GREEN and full native suite**

Run: `cmake --build build --target dsp_pedal_control_state_test && ctest --test-dir build --output-on-failure`

Expected: new test and all existing tests pass.

- [ ] **Step 5: Commit**

```sh
git add CMakeLists.txt core/include/dsp/control test/control/pedal_control_state_test.cpp
git commit -m "feat: add fixed realtime pedal control state"
```

### Task 2: Deterministic MIDI clock, tap tempo, and normalized MIDI translation

**Files:**
- Create: `targets/daisy/include/dsp/targets/daisy/control_event.hpp`
- Create: `targets/daisy/include/dsp/targets/daisy/tempo_controller.hpp`
- Create: `targets/daisy/include/dsp/targets/daisy/midi_adapter.hpp`
- Create: `targets/daisy/src/tempo_controller.cpp`
- Create: `targets/daisy/src/midi_adapter.cpp`
- Test: `test/targets/daisy/tempo_controller_test.cpp`
- Test: `test/targets/daisy/midi_adapter_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `using MonotonicMicros = std::uint64_t;`
- `TempoController::on_midi_start(MonotonicMicros) noexcept`
- `TempoController::on_midi_continue(MonotonicMicros) noexcept`
- `TempoController::on_midi_stop() noexcept`
- `TempoController::on_midi_clock(MonotonicMicros) noexcept`
- `TempoController::on_tap(MonotonicMicros) noexcept`
- `TempoController::update_timeout(MonotonicMicros) noexcept`
- `TempoController::tempo_bpm() const noexcept -> float`
- `TempoController::midi_authoritative() const noexcept -> bool`
- Produces `enum class MidiMessageKind : std::uint8_t { clock, start, stop, continue_playback, control_change, unsupported };`.
- Produces `struct MidiMessage { MidiMessageKind kind; std::uint8_t channel; std::uint8_t data1; std::uint8_t data2; bool valid; };`.
- Produces bounded `ControlEvent` variants for tempo/transport and normalized primary/secondary updates.
- `MidiAdapter::translate(const MidiMessage&, MonotonicMicros) noexcept -> std::optional<ControlEvent>`.
- Task 5 converts libDaisy `MidiEvent` to `MidiMessage`; this task remains host-testable.

- [ ] **Step 1: Write failing tempo tests**

Cover:
- default 120 BPM, not MIDI-authoritative;
- exactly 24 evenly spaced clocks at 120 BPM resolve to 120 BPM within 0.1;
- Start/Continue only become authoritative once a valid estimate exists;
- Stop releases authority immediately;
- at 20 BPM, timeout does not fire before `max(250 ms, 6*interval)`, but fires immediately after;
- non-monotonic clock timestamps are ignored without corrupting the previous estimate;
- taps at 500 ms produce 120 BPM after two taps;
- 199 ms and 3001 ms tap intervals are ignored;
- four-interval fixed window updates without allocation;
- tap does not replace BPM while valid running MIDI is authoritative; after Stop/timeout it can.

- [ ] **Step 2: Write failing MIDI translation tests**

Define project-owned message cases for Clock, Start, Stop, Continue, channel-1 CC20/CC21, wrong-channel CC, unsupported CC, Program Change, SysEx/unsupported, and malformed/invalid decoded input.

Assert:
- CC20/21 normalize `0..127 -> 0..1`;
- only channel 1 CC20/21 produce control changes;
- Clock/Start/Stop/Continue produce tempo/transport intent;
- unsupported/malformed input returns no event;
- 10,000 valid translations allocate no memory.

- [ ] **Step 3: Run RED**

Run: `cmake --build build --target dsp_tempo_controller_test dsp_midi_adapter_test`

Expected: targets or symbols missing.

- [ ] **Step 4: Implement tempo and translation**

Use fixed arrays/counters only. MIDI Clock uses 24 PPQN and a maximum 24-interval moving estimate. Freshness timeout is `max(250000 us, 6 * expected_clock_interval_us)`. Tap history stores at most four intervals.

Do not include libDaisy headers in these implementation files.

- [ ] **Step 5: Run GREEN and full suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: all tests pass.

- [ ] **Step 6: Commit**

```sh
git add CMakeLists.txt targets/daisy/include targets/daisy/src test/targets/daisy
git commit -m "feat: add deterministic pedal MIDI and tempo control"
```

### Task 3: Hothouse control mapping and debounce/deadband

**Files:**
- Create: `targets/daisy/include/dsp/targets/daisy/control_mapper.hpp`
- Create: `targets/daisy/src/control_mapper.cpp`
- Test: `test/targets/daisy/control_mapper_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `ControlMapper::update_pot1(float normalized) noexcept`
- `ControlMapper::update_pot2(float normalized) noexcept`
- `ControlMapper::update_toggle1(std::uint8_t position, MonotonicMicros now) noexcept`
- `ControlMapper::update_footswitch1(bool pressed, MonotonicMicros now) noexcept`
- `ControlMapper::update_footswitch2(bool pressed, MonotonicMicros now) noexcept`
- `ControlMapper::apply_midi_control(std::uint8_t cc, float normalized) noexcept`
- `ControlMapper::snapshot() const noexcept -> PedalControlSnapshot`
- `ControlMapper::apply_to(dsp::effects::DelayProcessor&, const PedalControlSnapshot&) noexcept`
- Consumes Task 2 `TempoController`; tap footswitch delegates valid rising edges to it.

- [ ] **Step 1: Write failing mapping tests**

Assert:
- Pot 1 normalized 0 -> 1 ms, 1 -> 2000 ms through `exp(log(2000)*x)`;
- Pot 2 normalized 0 -> 0 feedback, 1 -> 0.95;
- CC20 and CC21 use exactly the same binding path/results as Pot 1/Pot 2;
- normalized pot movement `<0.002` does not publish a new value;
- movement `>=0.002` does;
- Toggle positions 0/1/2 map to free/quarter/dotted-eighth;
- toggle or footswitch edges inside 20 ms are ignored;
- Footswitch 1 toggles bypass on valid debounced press edges only;
- Footswitch 2 sends tap observations on valid debounced press edges only;
- invalid pot values/positions fail closed;
- repeated rapid valid updates stay allocation-free.

- [ ] **Step 2: Run RED**

Run: `cmake --build build --target dsp_control_mapper_test`

Expected: missing target/types.

- [ ] **Step 3: Implement mapper**

Keep Hothouse pin numbers out of this class; it maps already-sampled logical controls. Use fixed state only. `apply_to()` calls existing `DelayProcessor::set_delay_ms`, `set_feedback`, `set_tempo_bpm`, and `set_subdivision`.

- [ ] **Step 4: Run GREEN and full suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: all tests pass.

- [ ] **Step 5: Commit**

```sh
git add CMakeLists.txt targets/daisy/include/dsp/targets/daisy/control_mapper.hpp targets/daisy/src/control_mapper.cpp test/targets/daisy/control_mapper_test.cpp
git commit -m "feat: map Hothouse controls to portable delay state"
```

### Task 4: Prepared pedal runtime and bypass crossfade

**Files:**
- Create: `targets/daisy/include/dsp/targets/daisy/pedal_runtime.hpp`
- Create: `targets/daisy/src/pedal_runtime.cpp`
- Test: `test/targets/daisy/pedal_runtime_test.cpp`
- Test: `test/targets/daisy/pedal_runtime_realtime_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `PedalRuntime::prepare(const dsp::ProcessSpec&, dsp::Processor&) -> bool`
- `PedalRuntime::set_control_snapshot(const PedalControlSnapshot&) noexcept`
- `PedalRuntime::process(const float* const* input, float* const* output, std::size_t channels, std::size_t frames) noexcept`
- Runtime owns prepared dry scratch sized exactly `channel_count * max_block_size`.
- Runtime applies the Task 3 Delay binding before processor invocation when configured with the reference delay processor.

- [ ] **Step 1: Write failing runtime tests**

Cover:
- invalid `ProcessSpec` preparation fails;
- 48 kHz / 48-frame stereo preparation succeeds;
- input is copied to output before in-place processor call;
- bypass=false yields processed signal;
- bypass=true settles to exact dry signal after a 5 ms ramp (240 samples at 48 kHz);
- switching bypass repeatedly does not restart an unchanged target;
- processor continues receiving input while fully bypassed: seed a delay tail during bypass, re-enable processing, and observe preserved tail rather than reset state;
- oversized frame/channel calls are no-ops;
- null channel pointers are skipped safely.

- [ ] **Step 2: Write failing realtime allocation test**

After `prepare()`, run 2,000 callbacks at 48 frames/stereo while alternating published bypass/control state outside the callback. Require zero allocations during `process()`.

- [ ] **Step 3: Run RED**

Run: `cmake --build build --target dsp_pedal_runtime_test dsp_pedal_runtime_realtime_test`

Expected: missing targets/types.

- [ ] **Step 4: Implement runtime**

Allocate scratch only in `prepare()`. Use fixed 5 ms linear bypass smoothing. Snapshot controls once per block; do not load mutable publication state inside the per-sample loop.

- [ ] **Step 5: Run GREEN and full suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: all tests pass including zero-allocation callback evidence.

- [ ] **Step 6: Commit**

```sh
git add CMakeLists.txt targets/daisy/include/dsp/targets/daisy/pedal_runtime.hpp targets/daisy/src/pedal_runtime.cpp test/targets/daisy/pedal_runtime*
git commit -m "feat: add allocation-free pedal runtime and bypass"
```

### Task 5: Pinned libDaisy/Hothouse firmware cross-build

**Files:**
- Modify: `flake.nix`
- Modify: `flake.lock`
- Create: `cmake/toolchains/daisy-arm-none-eabi.cmake`
- Create: `targets/daisy/CMakeLists.txt`
- Create: `targets/daisy/firmware/hothouse_main.cpp`
- Create: `targets/daisy/include/dsp/targets/daisy/hothouse_pins.hpp`
- Create: `targets/daisy/README.md`
- Create: `tools/check-target-boundary.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Firmware uses libDaisy Seed APIs, USB MIDI transport, ADC/switch/GPIO primitives, and the Task 4 runtime.
- Firmware configures 48 kHz and block size 48.
- Firmware initializes `DelayProcessor`, `PedalRuntime`, USB MIDI, Hothouse controls, and starts audio only after successful preparation.
- Hothouse pin definitions are target-local and derived from published hardware information; no GPL HothouseExamples source is copied or linked.

- [ ] **Step 1: Add failing architecture/build checks**

Create `tools/check-target-boundary.sh` to fail if `core/` or `effects/` contains includes/tokens matching libDaisy/Daisy/Hothouse/MIDI packet types.

Add a Nix `checks.daisy-firmware` placeholder that attempts to configure/build `targets/daisy` with the ARM toolchain and therefore fails before the firmware/toolchain files exist.

Expected RED command:
```sh
nix flake check --no-write-lock-file --print-build-logs
```

- [ ] **Step 2: Pin libDaisy and ARM toolchain**

Pin libDaisy at `facb66c76b5482918741695f4268b0185e474644` including its required git submodules. Use a repository flake input/fetch path that preserves submodules; do not use a GitHub tarball source that silently omits them.

Add `pkgsCross.arm-embedded.stdenv.cc` or the nixpkgs-equivalent `arm-none-eabi` GNU toolchain to the firmware check/dev tooling. If nixpkgs naming differs at the pinned revision, use the exact available cross package but keep it repo-pinned.

- [ ] **Step 3: Implement the firmware target**

`hothouse_main.cpp` must:
- initialize Daisy at full speed;
- configure 48 kHz and audio block size 48;
- configure USB MIDI;
- initialize ADCs/switches from target-local Hothouse pin definitions;
- keep MIDI parsing/control updates in the main/control loop, not the audio callback;
- publish validated state through `PedalControlState`;
- prepare the existing `DelayProcessor` and Task 4 runtime before starting audio;
- in the callback, adapt Daisy buffers to `PedalRuntime::process()` only;
- configure/document the selected denormal/flush-to-zero policy if libDaisy/Cortex-M7 support is verified during implementation; otherwise leave hardware denormal acceptance explicitly pending rather than guessing.

- [ ] **Step 4: Cross-build GREEN**

Run:
```sh
nix flake check --no-write-lock-file --print-build-logs
```

Expected:
- existing native/strict/faust/sanitizer checks remain green;
- target-boundary check passes;
- Daisy firmware produces an ARM ELF/BIN as a Nix check result without depending on runner-global SDKs.

- [ ] **Step 5: Inspect binary/config evidence**

Use `arm-none-eabi-size` and compile-time/static assertions or generated build metadata to verify the firmware target is ARM Cortex-M7 and contains the configured 48-frame/48-kHz target constants. No hardware-runtime claims are made here.

- [ ] **Step 6: Commit**

```sh
git add CMakeLists.txt flake.nix flake.lock cmake/toolchains targets/daisy tools/check-target-boundary.sh
git commit -m "feat: cross-build Daisy Hothouse delay firmware"
```

### Task 6: Documentation, hardware evidence contract, and merge-ready software gate

**Files:**
- Create: `docs/adr/0004-daisy-hothouse-control-boundary.md`
- Create: `targets/daisy/HARDWARE-VALIDATION.md`
- Modify: `README.md`
- Modify: issue #6 acceptance checklist only after evidence exists

**Interfaces:**
- Documents the software-complete vs hardware-pending boundary.
- Hardware validation records: board/Seed revision, firmware commit SHA, sample rate, block size, callback period, CPU/headroom evidence, USB MIDI smoke result, Pot1/Pot2/Toggle1/footswitch behavior, bypass switching, tap tempo, and denormal-decay observation.

- [ ] **Step 1: Document ADR and hardware checklist**

ADR-0004 records:
- fixed latest-value publication;
- USB MIDI scope;
- DelayProcessor reference binding;
- 48 kHz / 48-frame target;
- 5 ms software bypass;
- GPL HothouseExamples exclusion;
- real-hardware evidence required before closing #6.

`HARDWARE-VALIDATION.md` contains an unchecked table/template for every hardware-only claim. It must explicitly state that blank/unchecked items are not satisfied by cross-compilation.

- [ ] **Step 2: Commit documentation**

```sh
git add README.md docs/adr/0004-daisy-hothouse-control-boundary.md targets/daisy/HARDWARE-VALIDATION.md
git commit -m "docs: define Daisy Hothouse hardware validation gate"
```

- [ ] **Step 3: Run exact software gates**

Run:
```sh
nix flake check --no-write-lock-file --print-build-logs
git diff --check origin/master...HEAD
tools/check-target-boundary.sh
grep -RniE 'guitar-practice-system|JUCE' core effects targets/daisy --exclude='README.md' && exit 1 || true
```

Expected: all software checks pass; no forbidden cross-repo/JUCE dependency exists in implementation code.

- [ ] **Step 4: Whole-branch review**

Review `origin/master...HEAD` against #6 and the spec. Critical/Important findings require RED→GREEN fixes and a fresh full `nix flake check`.

Specifically inspect:
- whether libDaisy is actually pinned with submodules;
- whether any GPL HothouseExamples code/text was copied;
- whether audio callback transitively calls MIDI parsing, logging, allocation, or control sampling;
- whether control publication can tear or allocate;
- whether bypass scratch is bounded by prepared spec.

- [ ] **Step 5: Open PR with partial-acceptance wording**

PR body must state:
- software/native/cross-build gates completed;
- exact head SHA;
- #6 **does not close** until hardware validation is performed;
- list of unchecked hardware items from `HARDWARE-VALIDATION.md`.

Do not include `Closes #6` yet.

- [ ] **Step 6: Hosted CI exact-head gate**

Require hosted CI success on the exact PR head and no unresolved review threads before merge.

- [ ] **Step 7: Merge software slice without closing #6**

Merge after exact-head CI/review is green. Update #6 with the merge SHA and remaining hardware checklist. Keep #6 open.

- [ ] **Step 8: Hardware completion follow-up**

When a real Hothouse/Daisy target is available:
- flash exact firmware commit;
- record actual 48 kHz / 48-frame confirmation or discrepancy;
- verify Pot1/Pot2/Toggle1/Footswitch1/Footswitch2;
- verify USB MIDI Clock/transport/CC20/CC21;
- record `CpuLoadMeter` or equivalent callback/headroom evidence outside callback;
- run long feedback decay and inspect callback load/non-finite output for denormal issues;
- attach evidence to #6 and only then check hardware acceptance items / close #6.
