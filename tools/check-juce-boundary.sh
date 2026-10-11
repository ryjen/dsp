#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

pattern='#include[[:space:]]*[<"][^">]*juce|juce::'

# Self-test both routes: qualified types can appear without local JUCE includes.
for fixture in '#include <juce_audio_basics/juce_audio_basics.h>' 'juce::AudioSource'; do
  if ! printf '%s\n' "$fixture" | grep -qE "$pattern"; then
    echo "JUCE boundary detector failed to recognize fixture: $fixture" >&2
    exit 1
  fi
done

if grep -RniE "$pattern" \
    core effects analysis protocols targets \
    --include='*.hpp' --include='*.h' --include='*.cpp' --include='*.cc'; then
  echo "JUCE host dependency leaked outside hosts/juce" >&2
  exit 1
fi

exit 0
