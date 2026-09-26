# Validation contract

Repository validation is intentionally flake-driven. GitHub Actions should orchestrate Nix, not duplicate compiler, sanitizer, or test recipes.

## Deterministic CI gates

`nix flake check --no-write-lock-file --print-build-logs` evaluates:

- `native` — supported Release build plus the complete CTest suite.
- `strict` — Release build/test with `-Wall -Wextra -Wpedantic -Werror`.
- `sanitizers` — Linux-only Clang Debug build/test with AddressSanitizer and UndefinedBehaviorSanitizer.
- `faust-generated` — regenerate the committed tremolo Faust output and require byte equality.

The realtime allocation tests are ordinary CTest targets, so they are exercised by native, strict, and sanitizer checks rather than implemented as a separate CI recipe.

## Platform policy

The native, strict, and Faust-generation checks are exposed for every supported flake system. Sanitizer coverage is Linux-only because compiler runtime availability and behavior are platform-specific; this should not silently weaken the normal native/strict build on other systems.

## Evidence that is not a CI threshold

Callback-budget timings are recorded as review and hardware evidence. Shared-runner wall-clock measurements are not pass/fail gates because scheduler noise makes them unsuitable for stable realtime thresholds.

Daisy CPU headroom, actual audio block/sample-rate configuration, control latency, and hardware callback timing remain target evidence under issue #6 rather than generic CI requirements.

## Extension rule

New validation belongs in the flake when it is deterministic, reproducible, and meaningful across the declared platform. GitHub workflow YAML should remain a thin `nix flake check` entrypoint.
