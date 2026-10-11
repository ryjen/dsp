#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

pattern='#include[[:space:]]*[<"][^">]*(daisy|hothouse)|\b(daisy::|MidiEvent|MidiMessageType|MidiUsbHandler|MidiUartHandler)\b'

if grep -RniE "$pattern" core effects analysis --include='*.hpp' --include='*.h' --include='*.cpp' --include='*.cc'; then
  echo "target-specific Daisy/MIDI packet dependency leaked into core/effects/analysis" >&2
  exit 1
fi

exit 0
