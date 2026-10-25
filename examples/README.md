# Deterministic effects audio demonstration

This is a **synthetic plucked-string fixture**, *not* a recording of the
user's guitar, a song, an IR model, or physical pi-Stomp output*.

Run from a native Linux/macOS CMake build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target dsp_effects_audio_demo
./build/dsp_effects_audio_demo ./demo-out
# Listen to the three WAVs in order:
# demo-out/dry.wav
# demo-out/tremolo.wav
# demo-out/tremolo-delay.wav
# Afterward: rm -rf ./build ./demo-out  (only these generated local folders)
```

It uses the existing `dsp::native::render_offline()` and C++ processors,
with a 48 kHz/128-frame stereo pipeline:

1. Generate reproducible simple plucked notes, no copyrighted audio.
2. Apply existing `TremoloProcessor` (rate=4.3Hz, depth=0.78).
3. Apply existing `DelayProcessor` (330ms, feedback=0.42, mix=0.43).
4. Export 4-second 16-bit PCM stereo WAVs for A/B listening.

The executable is run under CTest and checks finite output, audible DSP
difference and successful WAV creation. This is an *offline* demonstration:
it does not measure realtime latency, LV2 plugin-host compatibility,
guitar input or hardware behavior.
