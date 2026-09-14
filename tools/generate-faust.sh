#!/usr/bin/env sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${1:-"$repo/effects/tremolo/generated/tremolo_faust.hpp"}

cd "$repo"
mkdir -p "$(dirname -- "$output")"

exec faust -i \
  -a faust/architectures/standalone-header.cpp \
  -lang cpp -light -nvi -inpl \
  -scn FaustDspBase -cn TremoloFaust \
  -o "$output" \
  effects/tremolo/faust/tremolo.dsp
