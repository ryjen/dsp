# Faust Tremolo Portable Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement tremolo as the first portable effect using a selectively adopted Faust-generated gain kernel behind the existing host-independent `Processor` contract.

**Architecture:** `effects/tremolo/faust/tremolo.dsp` is authoritative for modulation, smoothing, waveform morph, and gain generation. A native `TremoloProcessor` PIMPL owns lifecycle, lock-free targets, preallocated gain storage, and audio-channel application. Generated C++ is committed and standalone; Faust is required only for regeneration and its reproducibility check.

**Tech Stack:** C++20, CMake/Ninja/CTest, Faust 2.85.5 from pinned nixpkgs, Nix flakes.

**Spec:** `docs/superpowers/specs/2026-09-14-faust-tremolo-design.md`

## Global Constraints

- `dsp_core` must not include or depend on Faust types or headers.
- Normal native compilation must succeed from committed generated C++ without the Faust compiler installed.
- `process()` must remain `noexcept`, allocation-free, bounded, and free of filesystem/network/logging/blocking-lock work.
- Faust-generated code is never hand edited.
- JUCE, Daisy, MIDI packet formats, plugin UI, and hardware work remain out of scope.
- The offline/native harness remains the correctness and evidence surface.

---
### Task 1: Reproducible Faust generation surface

**Files:**
- Create: `effects/tremolo/faust/tremolo.dsp`
- Create: `faust/architectures/standalone-header.cpp`
- Create: `tools/generate-faust.sh`
- Create: `effects/tremolo/generated/tremolo_faust.hpp`
- Modify: `flake.nix`

**Interfaces:**
- Consumes: pinned `nixpkgs` input from `flake.lock`.
- Produces: standalone class `TremoloFaust` with UI zones `rate_hz`, `depth`, and `shape`; `tools/generate-faust.sh [output-path]`.

- [ ] **Step 1: Add the Faust source and architecture template**

Use a common `os.phasor(1.0, rate)`; derive sine and triangle waveforms from the same phase; smooth UI parameters; output only the gain stream.

Create the architecture template with exactly the official `faust/dsp/dsp.h`, `faust/gui/UI.h`, and `faust/gui/meta.h` includes followed by `<<includeIntrinsic>>` and `<<includeclass>>`.

- [ ] **Step 2: Add the deterministic generator**

`tools/generate-faust.sh` must `cd` to repository root and invoke:

```sh
faust -i -a faust/architectures/standalone-header.cpp -lang cpp -light -nvi -inpl \
  -cn TremoloFaust -o "$output" effects/tremolo/faust/tremolo.dsp
```
- [ ] **Step 3: Add pinned Faust to development/generation tooling**

Add `pkgs.faust` to `devShells.default.packages`. Add a separate `checks.<system>.faust-generated` derivation with `pkgs.faust` and `pkgs.diffutils`; it regenerates to a temporary path and runs `diff -u` against the committed header. Do not add Faust to the existing `checks.<system>.native.nativeBuildInputs`.

- [ ] **Step 4: Generate and verify the committed artifact**

Run:

```bash
nix develop --command tools/generate-faust.sh
nix develop --command tools/generate-faust.sh /tmp/tremolo_faust.hpp
diff -u effects/tremolo/generated/tremolo_faust.hpp /tmp/tremolo_faust.hpp
nix flake check --no-write-lock-file --print-build-logs
```

Expected: regeneration diff is empty; native and generated-code checks pass.

- [ ] **Step 5: Commit**

```bash
git add flake.nix effects/tremolo/faust faust/architectures tools/generate-faust.sh effects/tremolo/generated
git commit -m "build: add reproducible Faust tremolo generation"
```

### Task 2: Tremolo processor public contract

**Files:**
- Create: `effects/tremolo/include/dsp/effects/tremolo.hpp`
- Create: `effects/tremolo/src/tremolo.cpp`
- Create: `test/effects/tremolo_test.cpp`
- Modify: `CMakeLists.txt`
**Interfaces:**
- Consumes: `dsp::Processor`, `dsp::ProcessSpec`, `dsp::AudioBlock`, `dsp::AtomicFloat`, generated `TremoloFaust`.
- Produces: `dsp::effects::TremoloProcessor` with `set_rate_hz(float)`, `set_depth(float)`, `set_shape(float)` and matching getters.

- [ ] **Step 1: Write the failing behavioral test**

Create a test that constructs `TremoloProcessor`, sets depth to zero before `prepare({48000.0, 64, 2})`, renders deterministic stereo samples through `dsp::native::render`, and requires exact passthrough. Also require public setters to clamp rate to `[0.1,20]`, depth to `[0,1]`, and shape to `[0,1]`.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target dsp_tremolo_test
```

Expected: compilation fails because `dsp/effects/tremolo.hpp` and the target do not exist.

- [ ] **Step 3: Implement the minimal wrapper**

Public declaration:

```cpp
class TremoloProcessor final : public Processor {
public:
    TremoloProcessor();
    ~TremoloProcessor() override;
    void prepare(ProcessSpec spec) override;
    void reset() noexcept override;
    void process(AudioBlock block) noexcept override;
    void set_rate_hz(float value) noexcept;
    void set_depth(float value) noexcept;
    void set_shape(float value) noexcept;
};
```
The PIMPL owns `TremoloFaust`, a no-allocation `UI` implementation that captures the three slider zones, and `std::vector<float> gain_buffer` allocated in `prepare()` to `max_block_size`.

Each setter clamps then publishes via `AtomicFloat`. `process()` loads targets once per block, writes the captured zones, calls generated `compute()` for `frame_count`, then multiplies every sample in every channel by the shared gain buffer.

- [ ] **Step 4: Verify GREEN**

Run:

```bash
cmake --build build --target dsp_tremolo_test
ctest --test-dir build -R dsp_tremolo_test --output-on-failure
```

Expected: exact depth-zero passthrough and clamp checks pass.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt effects/tremolo/include effects/tremolo/src test/effects/tremolo_test.cpp
git commit -m "feat: add portable tremolo processor"
```

### Task 3: Deterministic DSP and transition evidence

**Files:**
- Modify: `test/effects/tremolo_test.cpp`

**Interfaces:**
- Consumes: `TremoloProcessor` and native render/fixture/comparison helpers.
- Produces: deterministic correctness evidence for the generated kernel and wrapper.

- [ ] **Step 1: Add failing cases for effect behavior**
Add one test executable with explicit return codes for:

1. silence remains zero at full depth;
2. DC input at depth one stays in `[0,1]` and spans at least `0.25` over a one-second render;
3. two renders from `reset()` with identical inputs/targets compare equal with `abs_tol=1e-6`, `rel_tol=1e-5`;
4. shape `0` and shape `1` produce outputs that differ by more than `1e-3` somewhere after the smoothing lead-in;
5. changing depth from `0` to `1` on DC input does not create an adjacent-sample jump greater than `0.1` during the next 2048 samples.

- [ ] **Step 2: Verify RED where the current wrapper is incomplete**

Run:

```bash
cmake --build build --target dsp_tremolo_test
ctest --test-dir build -R dsp_tremolo_test --output-on-failure
```

Expected: at least one new behavior fails until reset/target initialization and smoothing semantics are correct.

- [ ] **Step 3: Make the smallest implementation corrections**

Adjust only generated Faust source or wrapper lifecycle/target application required by the failing evidence. Regenerate generated code after any `.dsp` change; never edit generated output directly.

- [ ] **Step 4: Verify GREEN and generation reproducibility**

Run the tremolo test and `nix flake check --no-write-lock-file --print-build-logs`; both must pass.

- [ ] **Step 5: Commit**

```bash
git add effects/tremolo test/effects/tremolo_test.cpp
git commit -m "test: prove deterministic tremolo behavior"
```
### Task 4: Realtime allocation and callback-budget evidence

**Files:**
- Create: `test/effects/tremolo_realtime_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `TremoloProcessor`, `dsp::native::measure_callback_budget`, and the allocation-counter pattern from `test/core/realtime_allocation_test.cpp`.
- Produces: regression evidence that prepared tremolo processing is heap-free and measured at representative callback configurations.

- [ ] **Step 1: Write the allocation/budget test**

Prepare the processor before enabling the allocation counter. Use stack-owned channel pointer arrays and preallocated vectors outside the measured callback. Assert allocation count does not change across repeated `process()` calls.

Measure and print callback reports for:

```text
48 kHz, 64 frames, stereo, 2000 callbacks
96 kHz, 128 frames, stereo, 2000 callbacks
```

Require finite non-negative average duration and positive deadline/fraction; do not impose a machine-specific timing threshold.

- [ ] **Step 2: Verify the allocation detector is live**

Before the realtime region, deliberately allocate one heap object and require the counter to increase. This prevents a false-green replacement `operator new`.

- [ ] **Step 3: Run the test**

```bash
cmake --build build --target dsp_tremolo_realtime_test
ctest --test-dir build -R dsp_tremolo_realtime_test -V
```

Expected: zero allocations inside prepared processing and two callback-budget reports.
- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt test/effects/tremolo_realtime_test.cpp
git commit -m "test: add tremolo realtime evidence"
```

### Task 5: Record the Faust decision and verify exact head

**Files:**
- Create: `docs/adr/0002-selective-faust-adoption.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: generation/runtime/evidence results from Tasks 1-4.
- Produces: explicit architectural decision and exact-head merge evidence.

- [ ] **Step 1: Write ADR-0002**

Record status `Accepted`, context from #4, the decision to adopt Faust selectively for suitable DSP kernels, and consequences: generated artifacts are reproducible and committed; ordinary native compilation remains Faust-free; processor/control/host boundaries remain native C++; effects may still be hand-written when Faust adds no value.

- [ ] **Step 2: Update README**

Document the first portable effect, the generation command:

```bash
nix develop --command tools/generate-faust.sh
```

and the rule that generated headers are not edited by hand.

- [ ] **Step 3: Run exact-head verification**

Run from clean build directories:

```bash
rm -rf build review-build sanitize-build
nix flake check --no-write-lock-file --print-build-logs
nix develop --command bash -lc 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure'
```
Then run:

```bash
nix develop --command bash -lc 'cmake -S . -B review-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" && cmake --build review-build && ctest --test-dir review-build --output-on-failure'
nix develop --command bash -lc 'cmake -S . -B sanitize-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" && cmake --build sanitize-build && ctest --test-dir sanitize-build --output-on-failure'
git diff origin/master...HEAD --check
grep -RInE 'JUCE|Daisy|libDaisy|midi' core effects/tremolo/include effects/tremolo/src CMakeLists.txt || true
```

Expected: all tests pass in all three configurations, generation check is clean, diff check is clean, and no forbidden framework/packet coupling appears in runtime sources.

- [ ] **Step 4: Commit docs**

```bash
git add docs/adr/0002-selective-faust-adoption.md README.md
git commit -m "docs: record selective Faust adoption"
```

- [ ] **Step 5: Push, open PR, and require hosted exact-head CI**

Push `issue-4-faust-tremolo`, open a PR against `master` with `Closes #4`, verify no unresolved review threads, and merge only with expected-head SHA after GitHub Actions succeeds. After merge, require the `master` push workflow to succeed before cleaning the worktree and syncing epic #1.
