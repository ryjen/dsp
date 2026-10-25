#!/usr/bin/env bash
# Install two new LV2 bundles without root, overwrite, or service restarts.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: $0 BUILD_OUTPUT_DIRECTORY" >&2; exit 2
fi
[[ "$(uname -s)" == Linux && "$(uname -m)" == aarch64 ]] || exit 1
[[ "$(id -un)" == pistomp && "$HOME" == /home/pistomp ]] || {
  echo "Run only as the unprivileged pistomp user on the device (no sudo)." >&2; exit 1;
}
source_root="$(cd "$1" && pwd -P)/lib/lv2"
target="/home/pistomp/data/.lv2"
[[ -d "$source_root" && ! -L "$source_root" && ! -L "$target" ]] || {
  echo "Invalid source or symlinked destination" >&2; exit 1;
}
for effect in delay tremolo; do
  from="$source_root/micrantha-$effect.lv2"
  [[ -d "$from" && ! -L "$from" && -s "$from/micrantha-$effect.so" ]] || {
    echo "Invalid bundle: $from" >&2; exit 1;
  }
  readelf -h "$from/micrantha-$effect.so" | grep -q 'Machine:.*AArch64' || exit 1
  [[ ! -e "$target/micrantha-$effect.lv2" && ! -L "$target/micrantha-$effect.lv2" ]] || {
    echo "Existing bundle found; refusing overwrite: $effect" >&2; exit 1;
  }
done
mkdir -p -- "$target"
staging="$(mktemp -d "$target/.micrantha-stage.XXXXXXXX")"
installed=()
complete=0
cleanup() {
  if [[ "$complete" -eq 0 ]]; then
    for effect in "${installed[@]}"; do
      rm -rf -- "$target/micrantha-$effect.lv2"
    done
  fi
  rm -rf -- "$staging"
}
trap cleanup EXIT
for effect in delay tremolo; do
  cp -R -- "$source_root/micrantha-$effect.lv2" "$staging/"
done
for effect in delay tremolo; do
  mv -- "$staging/micrantha-$effect.lv2" "$target/micrantha-$effect.lv2"
  installed+=("$effect")
done
complete=1
echo "Installed only micrantha-delay.lv2 and micrantha-tremolo.lv2 into $target"
echo "Audio services were not restarted. Refresh MOD-UI only when interruption is safe."
