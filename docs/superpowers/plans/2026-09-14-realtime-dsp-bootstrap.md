# Realtime DSP Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a reproducible C++/CMake/Nix/CI foundation for `ryjen/dsp` while preserving the historical Go kata and deferring the actual realtime processor API to issue #3.

**Architecture:** Preserve the current Go implementation under `legacy/go-kata/`. Create only a behaviorless `dsp_core` interface target plus native smoke-test wiring, so build boundaries are proven without prematurely designing DSP contracts. Use a pinned Nix flake as the shared local/CI build surface.

**Tech Stack:** C++20, CMake, Ninja, CTest, Nix flakes (`nixos-26.05`), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-14-realtime-dsp-bootstrap-design.md`

## Global Constraints

- New production/realtime code must not depend on the historical Go kata.
- Core native build must not require Faust, JUCE, Daisy SDKs, hardware, or Go.
- No dynamic allocation, filesystem/network I/O, or blocking locks may be introduced into future audio callbacks; issue #2 documents but does not implement that API.
- CI correctness must not depend on uploaded GitHub Actions artifacts.
- Do not invent the processor/control API owned by issue #3.

---

### Task 1: Preserve and isolate the legacy Go kata

**Files:**
- Move: `pkg/*` -> `legacy/go-kata/pkg/*`
- Move: `test/*` -> `legacy/go-kata/test/*`
- Create: `legacy/go-kata/README.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: current repository layout at commit `ce3f7741c15e3748a06ecc126d1f965e556aed5a`
- Produces: an explicitly historical tree that no new build target references

- [ ] **Step 1: Record the original file inventory**

Run:
```bash
find pkg test -type f -print | sort
```
Expected: all current Go source/tests and fixture files are listed.

- [ ] **Step 2: Move the kata without editing its implementation**

Run:
```bash
mkdir -p legacy/go-kata
git mv pkg legacy/go-kata/pkg
git mv test legacy/go-kata/test
```

- [ ] **Step 3: Add legacy disposition documentation and replace the root README**

Document that the Go tree is preserved historical/reference material, known correctness concerns are intentionally not repaired in #2, and the new realtime build must not depend on it. Root README describes the control-plane/realtime-plane boundary and realtime constraints from the spec.

- [ ] **Step 4: Verify the move is preservation-only**

Run:
```bash
git diff --find-renames -- legacy/go-kata
```
Expected: source/test content is represented as renames plus new documentation, not rewritten Go implementation.

- [ ] **Step 5: Commit**

```bash
git add README.md legacy
git commit -m "chore: preserve legacy Go DSP kata"
```

### Task 2: Prove the native build boundary test-first

**Files:**
- Create: `core/include/dsp/core.hpp`
- Create: `test/native/native_smoke.cpp`
- Create: `CMakeLists.txt`

**Interfaces:**
- Consumes: no production DSP behavior
- Produces: CMake target `dsp_core` and CTest target `dsp_native_smoke`

- [ ] **Step 1: Write the native smoke test before build targets exist**

Create `test/native/native_smoke.cpp`:
```cpp
#include <dsp/core.hpp>

int main() {
    return 0;
}
```

- [ ] **Step 2: Verify RED**

Run:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```
Expected: configuration fails because the repository does not yet contain a root `CMakeLists.txt`.

- [ ] **Step 3: Add minimal behaviorless core/build wiring**

Create `core/include/dsp/core.hpp` containing only the namespace declaration boundary, and add CMake configuration that:
- requires CMake 3.25+;
- sets C++20 with extensions off;
- declares `dsp_core` as an `INTERFACE` library exposing `core/include`;
- enables testing;
- builds `dsp_native_smoke` linked to `dsp_core`;
- registers it with CTest.

- [ ] **Step 4: Verify GREEN**

Run:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: configure/build succeed and `dsp_native_smoke` passes.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt core test/native
git commit -m "build: add native C++ DSP baseline"
```

### Task 3: Add reproducible Nix build and quality check

**Files:**
- Create: `flake.nix`
- Create: `flake.lock`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: CMake/CTest baseline from Task 2
- Produces: `devShells.default` and `checks.<system>.native`

- [ ] **Step 1: Add build-directory ignores**

Append `build/` and `.cache/` while preserving `.idea`.

- [ ] **Step 2: Add flake definition**

Use `nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05"`; support `x86_64-linux`, `aarch64-linux`, `x86_64-darwin`, and `aarch64-darwin`; provide a dev shell with CMake, Ninja, Clang, clang-tools, Git, and coreutils. Define a native check derivation that runs CMake configure/build/CTest without Faust/JUCE/Daisy/Go.

- [ ] **Step 3: Lock dependencies**

Run:
```bash
nix flake lock
```
Expected: `flake.lock` is created with a pinned nixpkgs revision.

- [ ] **Step 4: Verify the reproducible shell**

Run:
```bash
nix develop --command bash -lc 'cmake -S . -B build-nix -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-nix && ctest --test-dir build-nix --output-on-failure'
```
Expected: native smoke test passes.

- [ ] **Step 5: Verify flake quality gate**

Run:
```bash
nix flake check
```
Expected: all declared checks pass.

- [ ] **Step 6: Commit**

```bash
git add .gitignore flake.nix flake.lock
git commit -m "build: add reproducible Nix DSP environment"
```

### Task 4: Add CI on the reproducible quality surface

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: `nix flake check`
- Produces: pull-request/push CI with no artifact dependency

- [ ] **Step 1: Add GitHub Actions workflow**

Configure checkout, Nix installation, and `nix flake check --print-build-logs` on pull requests and pushes to the default branch. Do not upload build artifacts.

- [ ] **Step 2: Validate workflow syntax locally**

Run a deterministic YAML parse using Python when PyYAML is available in the Nix shell, or use a simple repository policy check that the workflow contains the expected trigger and `nix flake check` command. The authoritative execution check remains GitHub Actions after push.

- [ ] **Step 3: Re-run the complete local gate**

Run:
```bash
rm -rf build build-nix
nix flake check --print-build-logs
nix develop --command bash -lc 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure'
```
Expected: both gates pass from clean build directories.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: verify realtime DSP bootstrap"
```

### Task 5: Exact-head review and issue/PR handoff

**Files:**
- Modify if review finds defects: only files created/changed by Tasks 1-4

**Interfaces:**
- Consumes: completed bootstrap branch
- Produces: reviewed exact-head branch ready for PR

- [ ] **Step 1: Review scope and architecture**

Run:
```bash
git diff origin/master...HEAD --stat
git diff origin/master...HEAD --check
git status --short
```
Expected: only bootstrap/legacy/docs/build/CI changes, no dirty files, no whitespace errors.

- [ ] **Step 2: Search for forbidden coupling**

Run:
```bash
grep -RInE 'JUCE|Daisy|libDaisy|faust' core test/native CMakeLists.txt || true
grep -RIn 'legacy/go-kata' core test/native CMakeLists.txt || true
```
Expected: no production native dependency on optional frameworks or legacy Go code.

- [ ] **Step 3: Run fresh exact-head verification**

Run:
```bash
rm -rf build build-nix
nix flake check --print-build-logs
nix develop --command bash -lc 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure'
```
Expected: all checks pass on current `HEAD`.

- [ ] **Step 4: Push branch and open PR for #2**

PR description must summarize legacy disposition, architecture boundary, reproducible local commands, verification evidence, and `Closes #2`.
