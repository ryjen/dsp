# Optional LV2 host for pi-Stomp evaluation

The optional LV2 adapter reuses the existing C++ delay and Faust-generated tremolo core. No DSP algorithm or frozen guitar-dsp.preset v1 change is required.

## Build and validate (Linux)

```sh
nix develop .#lv2
cmake -S . -B build-lv2 -G Ninja -DDSP_ENABLE_LV2_HOST=ON
cmake --build build-lv2
ctest --test-dir build-lv2 -R '^dsp_lv2_.*_host_test$' --output-on-failure
LV2_PATH="$PWD/build-lv2/lv2" lv2ls
```

The Linux flake check `checks.<system>.lv2-host` performs the host tests plus Lilv discovery. Bundles appear at `build-lv2/lv2/micrantha-delay.lv2` and `build-lv2/lv2/micrantha-tremolo.lv2`, each containing a .so, manifest.ttl and effect metadata.

## Contract

Audio is stereo: input ports 0/1, output 2/3. Delay controls at 4..8 are time milliseconds, feedback, mix, BPM, subdivision; subdivision rounds to 0=free,1=quarter,2=dotted-eighth,3=eighth,4=eighth-triplet,5=sixteenth. Tremolo controls at 4..6 are rate Hz, depth and shape.

Construction and DSP preparation occur during instantiate; activate applies current controls and resets effect state. Every run() uses bounded 2048-frame chunks with no allocation or I/O. NaN and infinity controls fall back to documented defaults. Outputs must be connected. The plugin declares LV2 inPlaceBroken as a **required** host feature and rejects instantiation when the host does not advertise support. A conforming host must not alias audio input to output ports (cross-port aliasing is not supported). The plugin uses no filesystem, network or control-plane code on the callback.

## Deployment boundary

pi-Stomp runs a Raspberry Pi OS based image. Build plugin shared libraries for the target's actual CPU/ABI, not your development x86_64 host. Copy entire .lv2 bundle directories and validate using Lilv and the actual MOD-UI host; do not replace its OS or assume plugin compatibility. The pi-Stomp manual documents factory bundles at /usr/lib/lv2 and user bundles at /home/pistomp/data/.lv2 but permissions and current image versions require verification.

Hardware and MOD-UI validation: #26. Optional custom Yocto image decision: #27. Host tests are *not* evidence of physical audio latency or stability.
