# Validation contract

## Explain it to a human

The repository uses one Nix command to check portable DSP, realtime safety,
plugin hosts and cross-built firmware. A green software test does **not** prove
that a physical pedal works.

## Canonical check matrix

Run `nix flake check --no-write-lock-file --print-build-logs`.

| Check | Evidence |
| --- | --- |
| `native` | CMake Release and all native CTest suites: processors, protocol, analysis and allocation |
| `strict` | CMake/CTest with `-Wall -Wextra -Wpedantic -Werror` |
| `faust-generated` | Regenerated tremolo code matches committed output |
| `target-boundary` | Embedded SDK/MIDI transport types do not leak into portable core |
| `juce-boundary` | JUCE types remain inside optional host adapter |
| `juce-host` | Optional JUCE adapter builds and its test passes |
| `pistomp-scripts` | Syntax-check native pi-Stomp build/install Bash scripts |
| `sanitizers` (Linux) | Clang ASan/UBSan build plus CTest |
| `daisy-firmware` (Linux) | Pinned ARM/libDaisy cross-build, ELF/bin and configured sample/block checks |
| `lv2-host` (Linux) | Build both LV2 effects, run host ABI tests and verify Lilv discovery |

The shared-runner callback budget is **observational**, not a timing pass/fail
threshold. Portable effect allocation tests do not automatically cover all
LV2 adapter behavior.

## CI and platforms

`.github/workflows/ci.yml` orchestrates the flake on a GitHub Ubuntu runner
and separately runs `checks.aarch64-linux.lv2-host` on an ARM64 hosted runner.
This verifies Linux ARM64 plugin code, **not** binaries deployable directly
to the Debian pi-Stomp image. See [pi-Stomp deployment](../targets/pistomp/README.md)
for native on-device packaging.

The flake declares `x86_64-linux`, `aarch64-linux`,
`x86_64-darwin` and `aarch64-darwin` evaluation targets; Linux-only checks
are conditionally exposed. Declaring a platform is not evidence every one
was executed. Hosted CI should remain least-privileged, with pinned actions
and checkout credentials disabled. Running untrusted PR code on a persistent
self-hosted runner requires a separate isolation review.

## Hardware gates remain separate

- **Daisy/Hothouse #6:** flash/boot; physical pots, switch, USB MIDI and codec;
  actual sample rate, block size, CPU/callback headroom, bypass and feedback
  decay/denormals. Follow [hardware validation](../targets/daisy/HARDWARE-VALIDATION.md).
- **pi-Stomp #26:** real MOD-UI/mod-host loading including required LV2
  feature support, pedal controls, round-trip audio latency, XRUNs, signal
  quality, reboot and rollback. Lilv discovery is not runtime compatibility.
- **JUCE release:** the host adapter passes tests but licensing and packaging
  of any future JUCE-linked binary require a separate decision (ADR 0007).

The DSP epic #1 stays open for Daisy hardware validation; pi-Stomp is an
independent optional target.

## Extension rule

Add deterministic CTest/flake checks first and keep GitHub YAML thin.
Use separate physical measurements for audio latency and stability.
Clean local build outputs after tests; Nix derivations already isolate builds.
