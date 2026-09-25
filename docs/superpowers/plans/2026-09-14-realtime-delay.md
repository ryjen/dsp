# Realtime Delay and Tempo-Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded native C++ delay processor with deterministic tempo subdivision resolution, fractional circular-buffer reads, stable feedback, smoothed live controls, and realtime evidence.

**Architecture:** `DelayProcessor` owns preallocated per-channel ring buffers behind the existing `dsp::Processor` interface. Normalized atomic controls are loaded once per block; delay time, feedback, and mix are smoothed without callback allocation. MIDI decoding remains outside the effect.

**Tech Stack:** C++20, CMake/Ninja, existing `dsp_core` and `dsp_native`, Nix CI.

**Spec:** `docs/superpowers/specs/2026-09-14-realtime-delay-design.md`

## Global Constraints

- Maximum delay: 2000 ms; minimum effective delay: one sample.
- Feedback is clamped to `[0, 0.95]`; mix to `[0, 1]`; BPM to `[20, 300]`.
- Delay-time changes ramp for 100 ms; feedback/mix changes ramp for 10 ms.
- No callback allocation, I/O, blocking locks, MIDI packet types, JUCE types, or Daisy types.
- Initial prepare/reset applies current targets immediately; only live target changes ramp.
- Non-finite scalar setter values are ignored, preserving the last legal published value.
## Review Focus

- Non-finite delay/feedback/mix/BPM inputs preserve the last finite value; Task 2 pins this.
- Unknown `DelaySubdivision` values resolve as `free` rather than producing an invalid multiplier; Task 1 pins this.
- Re-preparing at a different sample rate/channel count replaces and clears prepared storage deterministically; Task 2 pins this.
- Oversized blocks are ignored and null channel pointers are skipped without disturbing other channels; Task 2 pins this.
- Maximum legal feedback under DC/silence input remains finite and bounded over a long render; Task 3 pins this.

---

### Task 1: Deterministic delay-time and subdivision mapping

**Files:**
- Create: `effects/delay/include/dsp/effects/delay_time.hpp`
- Create: `test/effects/delay_time_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `enum class DelaySubdivision : std::uint32_t`.
- Produces `float resolve_delay_ms(float free_ms, float bpm, DelaySubdivision subdivision) noexcept`.
- Later processor code consumes only resolved milliseconds, never MIDI data.

- [ ] **Step 1: Write the failing mapping test**
Create `test/effects/delay_time_test.cpp` with direct assertions:

```cpp
#include <dsp/effects/delay_time.hpp>
#include <cmath>
using dsp::effects::DelaySubdivision;
using dsp::effects::resolve_delay_ms;

int main() {
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::free) != 250.0F) return 1;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::quarter) != 500.0F) return 2;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::dotted_eighth) != 375.0F) return 3;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::eighth) != 250.0F) return 4;
    if (std::abs(resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::eighth_triplet) - 166.66667F) > 1e-3F) return 5;
    if (resolve_delay_ms(250.0F, 120.0F, DelaySubdivision::sixteenth) != 125.0F) return 6;
    if (resolve_delay_ms(-10.0F, 120.0F, DelaySubdivision::free) != 1.0F) return 7;
    if (resolve_delay_ms(5000.0F, 120.0F, DelaySubdivision::free) != 2000.0F) return 8;
    if (resolve_delay_ms(250.0F, 1.0F, DelaySubdivision::quarter) != 2000.0F) return 9;
    if (resolve_delay_ms(250.0F, 1000.0F, DelaySubdivision::quarter) != 200.0F) return 10;
    const auto unknown = static_cast<DelaySubdivision>(999U);
    if (resolve_delay_ms(321.0F, 120.0F, unknown) != 321.0F) return 11;
    return 0;
}
```

Register `dsp_delay_time_test` in CMake.

- [ ] **Step 2: Run the test to verify RED**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target dsp_delay_time_test`

Expected: compile failure because `dsp/effects/delay_time.hpp` does not exist.

- [ ] **Step 3: Implement the pure mapping**

Create the header-only enum and resolver:

```cpp
enum class DelaySubdivision : std::uint32_t {
    free, quarter, dotted_eighth, eighth, eighth_triplet, sixteenth,
};

[[nodiscard]] inline float resolve_delay_ms(
    float free_ms, float bpm, DelaySubdivision subdivision) noexcept {
    const float safe_free = std::clamp(free_ms, 1.0F, 2000.0F);
    if (subdivision == DelaySubdivision::free) return safe_free;
    const float safe_bpm = std::clamp(bpm, 20.0F, 300.0F);
    float beats = 0.0F;
    switch (subdivision) {
        case DelaySubdivision::quarter: beats = 1.0F; break;
        case DelaySubdivision::dotted_eighth: beats = 0.75F; break;
        case DelaySubdivision::eighth: beats = 0.5F; break;
        case DelaySubdivision::eighth_triplet: beats = 1.0F / 3.0F; break;
        case DelaySubdivision::sixteenth: beats = 0.25F; break;
        default: return safe_free;
    }
    return std::clamp((60000.0F / safe_bpm) * beats, 1.0F, 2000.0F);
}
```

- [ ] **Step 4: Verify GREEN and the full suite**

Run: `cmake --build build --target dsp_delay_time_test && ctest --test-dir build --output-on-failure`

Expected: `dsp_delay_time_test` passes and all existing tests pass.

- [ ] **Step 5: Commit**

```sh
git add CMakeLists.txt effects/delay/include/dsp/effects/delay_time.hpp test/effects/delay_time_test.cpp
git commit -m "feat: add deterministic delay subdivision mapping"
```
### Task 2: Preallocated delay processor and integer-delay behavior

**Files:**
- Create: `effects/delay/include/dsp/effects/delay.hpp`
- Create: `effects/delay/src/delay.cpp`
- Create: `test/effects/delay_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes Task 1 `DelaySubdivision` and `resolve_delay_ms(...)`.
- Produces `dsp::effects::DelayProcessor : dsp::Processor`.
- Public controls: `set_delay_ms`, `set_feedback`, `set_mix`, `set_tempo_bpm`, `set_subdivision` plus matching getters.
- Static bounds: `max_delay_ms = 2000.0F`, `max_feedback = 0.95F`.

- [ ] **Step 1: Write the failing processor test**

Create `test/effects/delay_test.cpp` using the public API first:

```cpp
DelayProcessor delay;
if (delay.prepare({0.0, 64, 1})) return 1;
delay.set_delay_ms(10.0F);
delay.set_feedback(0.0F);
delay.set_mix(1.0F);
std::vector<float> impulse(1200, 0.0F);
impulse[0] = 1.0F;
auto result = dsp::native::render_offline(
    delay, {48000.0, 64, 1}, {impulse}, 64);
if (!result) return 2;
if (std::abs(result.channels[0][480] - 1.0F) > 1.0e-6F) return 3;
```

Extend the same test with feedback echoes at samples 480/960, exact dry passthrough, finite clamping, NaN/Inf setters preserving the prior value, re-prepare clearing state, oversized-block no-op, and null-channel isolation.

- [ ] **Step 2: Run the test to verify RED**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target dsp_delay_test`

Expected: compile failure because `dsp/effects/delay.hpp` and `DelayProcessor` do not exist.

- [ ] **Step 3: Implement minimal prepared storage and processing**

Implement a private `Impl` with:
```cpp
std::vector<std::vector<float>> ring;
std::size_t write_index{};
std::size_t capacity{};
ProcessSpec spec{};
LinearSmoother delay_samples;
LinearSmoother feedback;
LinearSmoother mix;
float last_delay_target{};
float last_feedback_target{};
float last_mix_target{};
bool prepared{};
```

`prepare()` calculates `ceil(sample_rate * 2.0) + 2` with overflow checks, allocates per-channel storage, and initializes smoothers directly from current published targets. `reset()` zeroes existing buffers and resets the write index without allocating.

For this task, integer delay positions may use the fractional-read helper with an integral position; Task 3 extends the evidence to fractional and live transitions.
The callback loads atomic controls once per block, starts a smoother only when its resolved target changed, and uses this bounded inner shape:

```cpp
for (std::size_t frame = 0; frame < block.frame_count; ++frame) {
    const float delay_samples = impl_->delay_samples.next();
    const float feedback = impl_->feedback.next();
    const float mix = impl_->mix.next();
    for (std::size_t channel = 0; channel < block.channel_count; ++channel) {
        float* samples = block.channel(channel);
        if (samples == nullptr) continue;
        const float input = samples[frame];
        const float delayed = impl_->read(channel, delay_samples);
        impl_->ring[channel][impl_->write_index] = input + delayed * feedback;
        samples[frame] = input * (1.0F - mix) + delayed * mix;
    }
    impl_->write_index = (impl_->write_index + 1) % impl_->capacity;
}
```

- [ ] **Step 4: Verify GREEN and the full suite**

Run: `cmake --build build --target dsp_delay_test && ctest --test-dir build --output-on-failure`

Expected: processor tests and all prior tests pass.

- [ ] **Step 5: Commit**

```sh
git add CMakeLists.txt effects/delay/include/dsp/effects/delay.hpp effects/delay/src/delay.cpp test/effects/delay_test.cpp
git commit -m "feat: add bounded realtime delay processor"
```

### Task 3: Fractional reads, tempo sync, and click-resistant live transitions

**Files:**
- Modify: `test/effects/delay_test.cpp`
- Modify: `effects/delay/src/delay.cpp`

**Interfaces:**
- Consumes Task 2 `DelayProcessor`.
- Keeps the public API unchanged.
- Confirms tempo/subdivision changes use the same delay-time smoothing path as free-time changes.
- [ ] **Step 1: Add failing behavioral evidence**

Extend `dsp_delay_test` with concrete fractional/transition cases:

```cpp
DelayProcessor fractional;
fractional.set_delay_ms(1.5F * 1000.0F / 48000.0F);
fractional.set_feedback(0.0F);
fractional.set_mix(1.0F);
// Render a short impulse and require the delayed energy to split
// linearly across the two neighboring output samples.

DelayProcessor synced;
synced.set_tempo_bpm(120.0F);
synced.set_subdivision(DelaySubdivision::quarter);
synced.set_feedback(0.0F);
synced.set_mix(1.0F);
// At 48 kHz, require the impulse at sample 24000.
```

Also cover a live free-time change and a live BPM change with adjacent-sample deltas below `0.25F`, silence preservation, and four seconds of finite DC output at feedback `0.95` bounded by `1/(1-0.95) + 1e-3`.

- [ ] **Step 2: Run the test to verify RED**

Run: `cmake --build build --target dsp_delay_test && ctest --test-dir build -R dsp_delay_test --output-on-failure`

Expected: at least the new fractional/transition assertions fail until fractional reads and target-change smoothing are complete.

- [ ] **Step 3: Implement fractional reads and target-change ramps**

Use a wrapped floating read position and linear interpolation:

```cpp
float read(std::size_t channel, float delay_samples) const noexcept {
    float pos = static_cast<float>(write_index) - delay_samples;
    while (pos < 0.0F) pos += static_cast<float>(capacity);
    const auto i0 = static_cast<std::size_t>(pos) % capacity;
    const auto i1 = (i0 + 1) % capacity;
    const float frac = pos - std::floor(pos);
    return ring[channel][i0] * (1.0F - frac) + ring[channel][i1] * frac;
}
```

When a resolved target changes, call `delay_samples.set_target(target_samples, std::lround(spec.sample_rate * 0.100))`; use `0.010` seconds for feedback/mix. Constant published targets must not restart a ramp.

- [ ] **Step 4: Verify GREEN and the full suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: all tests pass with deterministic results.
- [ ] **Step 5: Commit**

```sh
git add effects/delay/src/delay.cpp test/effects/delay_test.cpp
git commit -m "feat: smooth fractional delay and tempo sync"
```

### Task 4: Realtime allocation and callback-budget evidence

**Files:**
- Create: `test/effects/delay_realtime_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes the final Task 3 processor API.
- Uses existing `dsp::native::measure_callback_budget`.
- Produces no production API changes.

- [ ] **Step 1: Write the realtime evidence source before registration**

Create `test/effects/delay_realtime_test.cpp` with the same global allocation counter pattern as `tremolo_realtime_test.cpp`:

```cpp
DelayProcessor processor;
processor.set_delay_ms(375.0F);
processor.set_feedback(0.65F);
processor.set_mix(0.5F);
if (!processor.prepare({48000.0, 64, 2})) return 1;
processor.reset();

tracking.store(true, std::memory_order_release);
for (std::size_t callback = 0; callback < 2000; ++callback) {
    processor.process(block);
}
tracking.store(false, std::memory_order_release);
if (allocation_count.load(std::memory_order_relaxed) != 0) return 2;
```

Add finite, positive callback-budget reports for 48 kHz/64/stereo and 96 kHz/128/stereo with 2000 callbacks; no timing threshold.
- [ ] **Step 2: Run the target to verify RED**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target dsp_delay_realtime_test`

Expected: build failure because the target/source is not registered yet.

- [ ] **Step 3: Register the realtime test**

Add `dsp_delay_realtime_test`, link `dsp_native dsp_delay`, register with CTest, and apply the same GNU-only `-Wno-mismatched-new-delete` suppression used by the existing allocation-detector tests.

No production change is permitted unless the allocation test exposes a real callback allocation; if it does, reproduce and fix under RED→GREEN.

- [ ] **Step 4: Verify GREEN and the full suite**

Run: `cmake --build build --target dsp_delay_realtime_test && ctest --test-dir build --output-on-failure`

Expected: zero callback allocations, valid timing reports, and the complete suite green.

- [ ] **Step 5: Commit**

```sh
git add CMakeLists.txt test/effects/delay_realtime_test.cpp
git commit -m "test: add realtime delay evidence"
```
### Task 5: Documentation and exact-head release gates

**Files:**
- Modify: `README.md`
- Create: `docs/adr/0003-native-stateful-delay.md`

**Interfaces:**
- Records why delay is native C++ under ADR-0002 selective Faust policy.
- Documents normalized tempo/subdivision ownership and realtime bounds.
- No runtime API changes.

- [ ] **Step 1: Document the shipped architecture**

README: add the native delay alongside tremolo and state that MIDI clock decoding remains an adapter concern.

ADR-0003: record native C++ selection, prepared per-channel ring ownership, two-second bound, linear fractional interpolation, 100 ms tape-style delay ramp, 10 ms mix/feedback ramps, and deferred dual-head crossfade.

- [ ] **Step 2: Run exact-head verification**

Run all of:
```sh
nix flake check --no-write-lock-file --print-build-logs
cmake -S . -B build-exact -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-exact
ctest --test-dir build-exact --output-on-failure
cmake -S . -B build-warnings -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build build-warnings
ctest --test-dir build-warnings --output-on-failure
```
Then run sanitizer and coupling gates:
```sh
cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
git diff --check origin/master...HEAD
! grep -RniE 'JUCE|Daisy|midi(_| |::|packet)' effects/delay core/include
```

Expected: Nix, Debug, warning-as-error, and sanitizer suites pass; diff check and forbidden-coupling grep are clean.

- [ ] **Step 3: Commit documentation**

```sh
git add README.md docs/adr/0003-native-stateful-delay.md
git commit -m "docs: record native realtime delay architecture"
```

- [ ] **Step 4: Re-run the exact-head gates**

Repeat Step 2 after the documentation commit so every completion claim is tied to the final commit.

- [ ] **Step 5: Review and integration gate**

Review the full `origin/master...HEAD` diff against the spec and issue #5. Critical/Important findings require a RED→GREEN fix and a fresh full-suite run before PR creation. Push/create/merge only after hosted CI is green on the exact PR head.