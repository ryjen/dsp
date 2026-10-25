#!/usr/bin/env bash
# Build and validate LV2 bundles on the actual ARM64 target without installing them.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: $0 EMPTY_OUTPUT_DIRECTORY" >&2; exit 2
fi
[[ "$(uname -s)" == Linux && "$(uname -m)" == aarch64 ]] || {
  echo "Requires native Linux aarch64; Nix-store binaries cannot be copied to Debian." >&2; exit 1;
}
for cmd in cmake ninja c++ readelf lv2ls sha256sum; do
  command -v "$cmd" >/dev/null || { echo "Missing dependency: $cmd" >&2; exit 1; }
done
root="$(cd "$(dirname "$0")/../.." && pwd -P)"
mkdir -p -- "$1"
output="$(cd "$1" && pwd -P)"
[[ "$output" != / && "$output" != "$root" ]] || {
  echo "Unsafe output directory" >&2; exit 1;
}
[[ -z "$(find "$output" -mindepth 1 -maxdepth 1 -print -quit)" ]] || {
  echo "Output directory must be empty: $output" >&2; exit 1;
}
scratch="$(mktemp -d /tmp/micrantha-lv2-build.XXXXXXXX)"
trap 'rm -rf -- "$scratch"' EXIT
cmake -S "$root" -B "$scratch/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DDSP_ENABLE_LV2_HOST=ON -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_SKIP_RPATH=ON
cmake --build "$scratch/build" --target dsp_lv2_delay dsp_lv2_tremolo dsp_lv2_host_test
ctest --test-dir "$scratch/build" -R '^dsp_lv2_(delay|tremolo)_host_test$' --output-on-failure
cmake --install "$scratch/build" --prefix "$output"
bundle_root="$output/lib/lv2"
for effect in delay tremolo; do
  bundle="$bundle_root/micrantha-$effect.lv2"
  plugin="$bundle/micrantha-$effect.so"
  [[ -s "$plugin" && -s "$bundle/manifest.ttl" && -s "$bundle/$effect.ttl" ]] || {
    echo "Missing plugin output: $bundle" >&2; exit 1;
  }
  readelf -h "$plugin" | grep -q 'Machine:.*AArch64' || {
    echo "Not an aarch64 ELF: $plugin" >&2; exit 1;
  }
  if readelf -d "$plugin" | grep -Eq '(RPATH|RUNPATH).*\/nix\/store'; then
    echo "Nix linked artifact is not deployable to Debian: $plugin" >&2; exit 1;
  fi
  LV2_PATH="$bundle_root" lv2ls | grep -Fxq "https://micrantha.com/lv2/$effect" || {
    echo "Lilv could not discover plugin: $effect" >&2; exit 1;
  }
done
(
  cd "$output"
  find lib/lv2 -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
  printf 'source_commit=%s\narchitecture=aarch64\n' "$(git -C "$root" rev-parse HEAD 2>/dev/null || echo unknown)" > BUILD-INFO.txt
  cat /etc/os-release >> BUILD-INFO.txt
)
echo "Native LV2 bundles validated: $bundle_root"
echo "The running pedal was not modified."
