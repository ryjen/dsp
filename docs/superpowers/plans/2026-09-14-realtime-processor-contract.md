# Realtime Processor Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Define and prove a host-independent realtime processor contract with allocation-free control primitives, deterministic offline rendering, golden/vector comparison, and callback-budget reporting.

**Architecture:** `dsp_core` stays header-only and owns only realtime-safe contracts/primitives. `dsp_native` owns non-realtime allocation, fixture generation, vector comparison, and timing; future JUCE/Daisy adapters depend on the same core contract without changing effect implementations.

**Tech Stack:** C++20, CMake 3.25+, Ninja, CTest, Nix flakes.

**Spec:** `docs/superpowers/specs/2026-09-14-realtime-processor-contract-design.md`

## Global Constraints

- Core code must not depend on JUCE, Daisy, MIDI packet formats, filesystem/network APIs, or the historical Go kata.
- `process()`, `reset()`, atomic parameter reads/writes, and smoothing operations are bounded and `noexcept`.
- Dynamic allocation is permitted during preparation/native-host setup, never during the realtime callback.
- Audio buffers are caller-owned planar `float` channels; processors do not retain block pointers.
- Native timing reports evidence only; #3 does not add a flaky wall-clock pass/fail threshold.
- Golden/vector comparisons use explicit absolute and relative tolerances.

---

### Task 1: Define audio/process/processor contracts test-first

**Files:**
- Create: `core/include/dsp/process_spec.hpp`
- Create: `core/include/dsp/audio_block.hpp`
- Create: `core/include/dsp/processor.hpp`
- Modify: `core/include/dsp/core.hpp`
- Create: `test/core/processor_contract_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `dsp::ProcessSpec`, `dsp::AudioBlock`, and abstract `dsp::Processor`.
- `ProcessSpec::valid() const noexcept -> bool`.
- `AudioBlock::channel(std::size_t) const noexcept -> float*`.
- `Processor::prepare(const ProcessSpec&) -> bool`, `reset() noexcept`, `process(AudioBlock) noexcept`.

- [ ] **Step 1: Write the failing contract test**

Create `test/core/processor_contract_test.cpp` that checks:

```cpp
#include <dsp/core.hpp>
#include <array>

int main() {
    dsp::ProcessSpec valid{48000.0, 64, 2};
    if (!valid.valid()) return 1;
    if (dsp::ProcessSpec{0.0, 64, 2}.valid()) return 2;
    if (dsp::ProcessSpec{48000.0, 0, 2}.valid()) return 3;
    if (dsp::ProcessSpec{48000.0, 64, 0}.valid()) return 4;

    std::array<float, 4> left{};
    std::array<float, 4> right{};
    float* channels[]{left.data(), right.data()};
    dsp::AudioBlock block{channels, 2, 4};
    if (block.channel(0) != left.data()) return 5;
    if (block.channel(1) != right.data()) return 6;
    return 0;
}
```

Register `dsp_processor_contract_test` with CTest before the headers exist.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Expected: compile failure because `ProcessSpec` and `AudioBlock` are not defined.

- [ ] **Step 3: Implement the minimal contracts**

Implement:

```cpp
struct ProcessSpec {
    double sample_rate{};
    std::size_t max_block_size{};
    std::size_t channel_count{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return sample_rate > 0.0 && max_block_size > 0 && channel_count > 0;
    }
};
```

```cpp
struct AudioBlock {
    float* const* channels{};
    std::size_t channel_count{};
    std::size_t frame_count{};

    [[nodiscard]] float* channel(std::size_t index) const noexcept {
        return channels[index];
    }
};
```

```cpp
class Processor {
public:
    virtual ~Processor() = default;
    virtual bool prepare(const ProcessSpec& spec) = 0;
    virtual void reset() noexcept = 0;
    virtual void process(AudioBlock block) noexcept = 0;
};
```

Update `core.hpp` as the umbrella include.

- [ ] **Step 4: Verify GREEN**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: existing smoke test and contract test pass.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt core test/core/processor_contract_test.cpp
git commit -m "feat: define realtime processor contract"
```

### Task 2: Add bounded parameter handoff and smoothing primitives

**Files:**
- Create: `core/include/dsp/atomic_float.hpp`
- Create: `core/include/dsp/linear_smoother.hpp`
- Modify: `core/include/dsp/core.hpp`
- Create: `test/core/control_primitives_test.cpp`
- Create: `test/core/realtime_allocation_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `dsp::AtomicFloat::store(float) noexcept`, `load() const noexcept`.
- Produces: `dsp::LinearSmoother::reset(float) noexcept`, `set_target(float, std::size_t) noexcept`, `next() noexcept`, `value() const noexcept`, `smoothing() const noexcept`.

- [ ] **Step 1: Write failing control tests**

`control_primitives_test.cpp` must verify:

```cpp
dsp::AtomicFloat parameter{0.25F};
if (parameter.load() != 0.25F) return 1;
parameter.store(0.75F);
if (parameter.load() != 0.75F) return 2;

dsp::LinearSmoother smoother;
smoother.reset(0.0F);
smoother.set_target(1.0F, 4);
const float expected[]{0.25F, 0.5F, 0.75F, 1.0F};
for (float value : expected) {
    if (std::abs(smoother.next() - value) > 1.0e-6F) return 3;
}
if (smoother.smoothing()) return 4;
```

Also verify `set_target(x, 0)` snaps immediately.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build
```

Expected: compile failure because the primitives do not exist.

- [ ] **Step 3: Implement `AtomicFloat` and `LinearSmoother`**

Use `std::bit_cast<std::uint32_t>` and `std::atomic<std::uint32_t>` for the float payload. Add:

```cpp
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
```

Use release store / acquire load. `LinearSmoother` stores only current/target/step/remaining sample count; no containers or allocations.

- [ ] **Step 4: Verify control tests GREEN**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: control tests pass.

- [ ] **Step 5: Add allocation-guard regression test**

Create a test-only `TrackingProcessor` using `AtomicFloat` and `LinearSmoother`. Override global `operator new/new[]` in the test executable to increment a counter only while a `tracking` atomic flag is enabled. Allocate buffers before enabling tracking, then call `process()` and assert the allocation count remains zero.

The processor loop is:

```cpp
void process(dsp::AudioBlock block) noexcept override {
    smoother_.set_target(target_.load(), block.frame_count);
    for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
        const float gain = smoother_.next();
        for (std::size_t channel = 0; channel < block.channel_count; ++channel) {
            block.channel(channel)[frame] *= gain;
        }
    }
}
```

- [ ] **Step 6: Run allocation test**

Run:

```bash
cmake --build build
ctest --test-dir build -R realtime_allocation --output-on-failure
```

Expected: zero callback allocations.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt core test/core
git commit -m "feat: add realtime-safe control primitives"
```

### Task 3: Build deterministic native offline rendering and comparison

**Files:**
- Create: `hosts/native/include/dsp/native/offline_renderer.hpp`
- Create: `hosts/native/include/dsp/native/sample_compare.hpp`
- Create: `hosts/native/include/dsp/native/fixtures.hpp`
- Create: `hosts/native/src/offline_renderer.cpp`
- Create: `test/native/offline_renderer_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces static library target `dsp_native`, linking `dsp_core`.
- Produces `dsp::native::RenderError`, `RenderResult`, and `render_offline(...)`.
- Produces `samples_close(expected, actual, abs_tol, rel_tol) noexcept`.
- Produces deterministic `make_silence`, `make_constant`, `make_impulse`, `make_sine` helpers.

- [ ] **Step 1: Write failing offline render tests**

Define a test-only passthrough `Processor`. Test that a 10-frame stereo input rendered with `max_block_size=4` produces callback frame sizes `4,4,2` and output exactly matches input.

Also test invalid inputs:

```text
block_size == 0                  -> invalid_block_size
block_size > spec.max_block_size -> invalid_block_size
channel count mismatch           -> invalid_input
unequal channel lengths           -> invalid_input
processor prepare failure         -> prepare_failed
```

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build
```

Expected: compile failure because native renderer APIs do not exist.

- [ ] **Step 3: Implement renderer and fixtures**

`render_offline` must:

1. validate spec/input/block size;
2. copy input into caller-independent output storage;
3. allocate the channel pointer array once before processing;
4. call `processor.prepare(spec)` and `processor.reset()` before rendering;
5. update pointer values for each block without resizing the pointer vector;
6. invoke `processor.process(AudioBlock{...})` for each chunk;
7. return output and `RenderError::none`.

All `std::vector` work belongs to the native host, outside `Processor::process()`.

- [ ] **Step 4: Add comparison and fixture tests**

Test the policy:

```cpp
const std::vector<float> expected{1.0F, 2.0F};
const std::vector<float> close{1.000001F, 1.999999F};
if (!dsp::native::samples_close(expected, close, 1.0e-5F, 1.0e-5F)) return 1;
```

Verify a deterministic impulse and sine fixture have expected sizes/anchor values.

- [ ] **Step 5: Verify GREEN**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all core/native tests pass without audio hardware.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt hosts/native test/native
git commit -m "feat: add deterministic offline DSP harness"
```

### Task 4: Add callback-budget reporting and architecture decision record

**Files:**
- Create: `hosts/native/include/dsp/native/callback_budget.hpp`
- Create: `hosts/native/src/callback_budget.cpp`
- Create: `test/native/callback_budget_test.cpp`
- Create: `docs/adr/0001-realtime-processor-contract.md`
- Modify: `README.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `CallbackBudgetReport` with callback count, elapsed seconds, average seconds/callback, deadline seconds, and average deadline fraction.
- Produces `measure_callback_budget(Processor&, ProcessSpec, frame_count, callback_count)` returning a structured result.

- [ ] **Step 1: Write failing budget-report test**

Use a no-op processor and verify:

```text
invalid spec/frame_count/callback_count -> invalid result
valid 48 kHz / 64-frame run            -> callbacks match request
deadline_seconds                        -> 64 / 48000
elapsed/average values                  -> non-negative
```

Print the report to stdout as diagnostic evidence, but do not assert a maximum duration.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build
```

Expected: compile failure because the budget API does not exist.

- [ ] **Step 3: Implement bounded measurement**

Allocate zeroed planar buffers and pointer storage before starting the timer. Call `prepare/reset` before timing. Time exactly `callback_count` calls to `process()` using `std::chrono::steady_clock`.

Compute:

```text
deadline_seconds = frame_count / sample_rate
average_seconds = elapsed_seconds / callback_count
average_deadline_fraction = average_seconds / deadline_seconds
```

- [ ] **Step 4: Verify budget test GREEN**

Run:

```bash
cmake --build build
ctest --test-dir build -R callback_budget --output-on-failure -V
```

Expected: test passes and emits timing evidence.

- [ ] **Step 5: Document the ADR and README boundary**

ADR must record:

- non-owning planar block rationale;
- virtual processor rationale and when to reconsider it;
- lifecycle/realtime method split;
- lock-free target + processor-owned fixed parameter sets;
- smoothing responsibility;
- native host allocation/timing ownership;
- no JUCE/Daisy types in core;
- explicit rule that future effects repeat allocation-guard tests.

Update README with links to the contract headers, native harness, ADR, and #4 as the next effect slice.

- [ ] **Step 6: Run full local gate and commit**

Run:

```bash
rm -rf build
nix flake check --no-write-lock-file --print-build-logs
nix develop --command bash -lc 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure'
```

Then:

```bash
git add CMakeLists.txt README.md docs/adr hosts/native test/native
git commit -m "feat: report DSP callback budget"
```

### Task 5: Exact-head review, CI, and issue handoff

**Files:**
- Modify only files introduced/changed by Tasks 1-4 if review finds defects.

**Interfaces:**
- Produces a reviewed exact-head branch suitable for PR against `master`.

- [ ] **Step 1: Review scope and dependency direction**

Run:

```bash
git diff origin/master...HEAD --stat
git diff origin/master...HEAD --check
git status --short
grep -RInE 'JUCE|Daisy|libDaisy|midi.h|legacy/go-kata' core hosts/native test/core test/native CMakeLists.txt || true
```

Expected: no forbidden production dependency; docs may mention future frameworks only where intentional.

- [ ] **Step 2: Inspect exception/allocation-sensitive signatures**

Run:

```bash
grep -RInE 'process\(|reset\(|store\(|load\(|next\(' core/include/dsp
```

Confirm realtime-facing methods are `noexcept`, and `process()` takes only non-owning block state.

- [ ] **Step 3: Run fresh exact-head verification**

Run:

```bash
rm -rf build
nix flake check --no-write-lock-file --print-build-logs
nix develop --command bash -lc 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure'
git diff --check origin/master...HEAD
```

Expected: all declared checks pass and worktree is clean.

- [ ] **Step 4: Push and open PR**

Push `issue-3-processor-contract` and create a PR with `Closes #3`, exact-head verification evidence, and parent epic #1.

- [ ] **Step 5: Review GitHub exact-head CI/review state**

Require successful CI on the exact PR head and no unresolved review threads before merge. If a defect surfaces, fix it in the same isolated worktree, rerun local exact-head verification, push, and repeat.

- [ ] **Step 6: Merge with expected-head protection and verify master**

Merge only against the verified head SHA. Confirm post-merge `master` CI is successful, #3 closes, and update epic #1 so #4 becomes the next critical-path item.
