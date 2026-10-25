# Offline DSP WAV renderer

The DSP repository owns a native executable, `dsp`, that processes an
existing WAV through the **same portable delay and tremolo processors**.
Guitar Practice System controls it as a separate, optional subprocess; no
DSP algorithms or new preset schema are copied into Python.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target dsp_render_cli
ctest --test-dir build -R dsp_render_cli_test --output-on-failure
./build/dsp render guitar.wav --effects tremolo,delay --output processed.wav
```

Supported format: little-endian RIFF PCM16 WAV, mono/stereo, 8–192 kHz,
maximum 64 MiB input PCM. Output is PCM16 WAV at the source rate/channels.
No resampling, metadata preservation, tail extension or normalization.
Unsupported formats fail without creating output; convert other formats
before using this renderer. An existing output file is never overwritten.

## Parameters and effect order

```sh
./build/dsp render guitar.wav --output delay.wav \
  --delay-ms 330 --feedback 0.42 --mix 0.43 --tempo 120 --subdivision free

./build/dsp render guitar.wav --output tremolo.wav --effects tremolo \
  --tremolo-rate 4.3 --tremolo-depth 0.78 --tremolo-shape 0.25

./build/dsp render guitar.wav --output guitar-processed.wav \
  --preset practice.preset --effects tremolo,delay
```

Available chains: `none`, `delay` (default), `tremolo`,
`tremolo,delay`, `delay,tremolo`. Delay settings are inline *or* from
an exact canonical `guitar-dsp.preset` v1 JSON file, not both.
This frozen contract describes the delay only; tremolo CLI flags remain
independent.

## guitarctl integration

```sh
guitarctl --workspace /path/to/recordings render guitar.wav \
  --preset practice.preset --effects tremolo,delay \
  --output guitar-processed.wav --engine /path/to/dsp/build/dsp
```

`guitarctl` validates and canonicalizes the v1 preset before invoking
the existing native executable; recording, preset and output paths are
workspace-relative. You can run `dsp render` independently without Python.
Use `guitarctl render --help` for command details.

See [audio demo](../../examples/README.md) for the synthetic sample.
Neither this CLI nor its tests prove live audio, pi-Stomp, Daisy, or
Boss RC-3 compatibility.
