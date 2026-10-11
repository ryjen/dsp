#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

pattern='#include[[:space:]]*[<"][^">]*juce|\\bjuce::'

if grep -RniE "$pattern" \
    core effects analysis protocols targets \
    --include='*.hpp' --include='*.h' --include='*.cpp' --include='*.cc'; then
  echo "JUCE host dependency leaked outside hosts/juce" >&2
  exit 1
fi

exit 0
