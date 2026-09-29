# Daisy / Hothouse target

This target cross-builds the existing portable DSP stack for a stereo
Cleveland Music Co. Hothouse carrier with a Daisy Seed/Seed3.

## Software contract

- 48 kHz sample rate, 48-frame audio blocks.
- Existing `DelayProcessor`; no target-specific effect fork.
- Delay history is caller-owned SDRAM so the two-second stereo buffer does not
  consume the default internal SRAM heap.
- USB MIDI: Clock, Start/Stop/Continue, channel-1 CC20/CC21.
- Hothouse Pot 1/2, Toggle 1, and Footswitch 1/2 are target adapters only.
- Audio callback performs DSP/bypass/CPU timing only. MIDI parsing and control
  sampling stay in the main loop.

## Source boundaries

`libDaisy` is pinned by revision and fixed-output hash in `flake.nix`.
The Hothouse connection facts are derived from Cleveland Music Co.'s
CC-BY-SA-4.0 open-hardware schematic in
`clevelandmusicco/open-source-pedals`. The GPL-3.0
`clevelandmusicco/HothouseExamples` firmware repository is not copied,
linked, vendored, or used as a build dependency.

Cross-compilation is software evidence only. Hardware runtime validation is a
separate acceptance gate.
