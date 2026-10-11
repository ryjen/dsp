# Validation contract

Repository validation is intentionally flake-driven. GitHub Actions should orchestrate Nix, not duplicate compiler, sanitizer, or test recipes.

## Canonical deterministic checks

The repository's CI entrypoint is:

```sh
nix flake check --no-write-lock-file --print-build-logs
```

The current `flake.nix` defines:

| Check | Scope and evidence |
| --- | --- |
| `native` | Release CMake build and complete native CTest suite, including realtime allocation, contract, protocol, DSP, analysis and adapter tests |
| `strict` | Release CMake build and CTest with `-Wall -Wextra -Wpedantic -Werror` |
| `sanitizers` | Linux-only Clang Debug build and CTest with AddressSanitizer and UndefinedBehaviorSanitizer |
| `faust-generated` | Regenerate committed tremolo Faust output and require byte-identical source |
| `target-boundary` | Reject forbidden embedded SDK/target dependencies in portable code |
| `juce-boundary` | Reject forbidden JUCE dependencies outside its optional host integration |
| `juce-host` | Opt-in JUCE adapter builds and passes its host test with pinned Nix dependency |
| `daisy-firmware` | Linux-only pinned ARM/libDaisy cross-build plus ELF target, binary and configured sample/block checks |

The realtime allocation and bounded performance analyzer tests run as regular CTest targets under `native`, `strict`, and (on Linux) `sanitizers`. The JUCE host check is separate: normal portable builds do not link JUCE.

## Platform and trust policy

The flake declares supported evaluation systems `x86_64-linux`, `aarch64-linux`, `x86_64-darwin`, and `aarch64-darwin`. Linux-specific sanitizer and Daisy cross-build checks are added only on Linux. The GitHub CI workflow currently runs the flake checks on an Ubuntu runner; a declared flake output is not evidence that every listed platform has executed it.

The runner executes untrusted repository content and should remain least-privileged, with source checkout and Nix dependencies pinned. Do not move fork-PR execution onto a persistent trusted self-hosted runner without an explicitly reviewed isolation and credential boundary.

The optional JUCE check validates only the adapter seam, not a distributable desktop application. No JUCE-linked binaries are released by this repository; a later distribution requires an explicit licensing and packaging decision (ADR 0007).

## Evidence that is not a CI threshold

Callback-budget timings are recorded as review and hardware evidence. Shared-runner wall-clock measurements are not pass/fail gates because scheduler noise makes them unsuitable for stable realtime thresholds.

The `daisy-firmware` check establishes cross-compilability, expected firmware configuration and artifact structure, **not** physical behavior. Daisy boot, sample rate/block size as actually observed, stereo channels, controls, USB MIDI, CPU/callback headroom, bypass/tail continuity, and long feedback-decay/denormal behavior remain hardware-only evidence under issue #6 and `targets/daisy/HARDWARE-VALIDATION.md`. Keep #6 and parent #1 open until those observations exist.

## Extension rule

New validation belongs in the flake when it is deterministic, reproducible, and meaningful across the declared platform. GitHub workflow YAML should remain a thin `nix flake check` entrypoint; preserve exact-revision test evidence and do not replace independent physical hardware qualification with software-only checks.
