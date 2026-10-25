# pi-Stomp: ARM64 software integration and physical evidence

Status: **native-build tooling available; real hardware not yet validated** (#26).

## Target contract

The current upstream pi-Stomp image uses Debian Trixie **arm64**, JACK2,
mod-host, MOD-UI and a real-time kernel. It supports the v2 and v3 pedal
generations, but record the actual Pi board and image, not an inferred one.
MOD-UI runs at `http://pistomp.local`. The current user plugin directory is
`/home/pistomp/data/.lv2`, independent from the factory plugins at
`/usr/lib/lv2`. This is a *target baseline*, not a device verification.

Reference: https://treefallsound.github.io/pistomp-manual/developers/building-images/
and https://treefallsound.github.io/pistomp-manual/using/mod-ui/.

## Build and validate on the *actual* device

Run as the `pistomp` user after authorizing dependency installation:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends -y \
    git build-essential cmake ninja-build lv2-dev lilv-utils binutils util-linux
git clone https://github.com/ryjen/dsp.git
cd dsp
bash targets/pistomp/build-bundles.sh /tmp/micrantha-lv2
```

The script deliberately rejects non-AArch64 hosts, requires an empty
staging directory, builds both plugins with the system compiler rather
than Nix store-linked binaries, runs LV2 ABI smoke tests, checks the ELF
architecture and Lilv discovery, and produces `SHA256SUMS` and
`BUILD-INFO.txt`. It cleans its temporary build dir. It **does not**
deploy or restart any audio process.

The GitHub ARM64 Nix CI gate verifies architecture-native LV2 execution;
it is not a Debian Trixie package and must not be copied to the device.

## Install without replacing the OS

When you are not actively playing or depending on the current pedalboard:

```sh
bash targets/pistomp/install-bundles.sh /tmp/micrantha-lv2
```

The script only runs as unprivileged `pistomp` on aarch64 Linux. It
refuses overwriting an existing bundle and places only the two new
`.lv2` directories beneath `/home/pistomp/data/.lv2`. It never writes
to `/usr/lib/lv2`, restarts mod-host, edits a pedalboard or reboots.
If the image uses different plugin paths, investigate the live service's
LV2_PATH configuration before executing; do not assume compatibility.

After checking that an audio interruption is acceptable, refresh plugin
discovery in MOD-UI and verify both plugins **load and run**. In
particular check that mod-host advertises the required LV2
`inPlaceBroken` feature; `lv2ls` discovery alone does not prove it.

## Hardware evidence checklist (not yet supplied)

| Check | Evidence required |
| --- | --- |
| Identity | Board and pi-Stomp revision, OS/kernel, `dpkg --print-architecture`, JACK, MOD-UI and mod-host versions |
| Controls | MOD-UI discovers and loads both URIs; free/tempo subdivision and tremolo parameters; footswitch/encoder mappings and stored state |
| Performance | Input-to-output measured round-trip latency at recorded JACK sample rate/periods; CPU margin and XRUN counts during sustained use |
| Signal | Dry/bypass operation, delayed tails, no clipping or abnormal feedback, long decay/denormal behavior |
| Recovery | Reboot, preset persistence, package downgrade/removal with saved pedalboards |
| Safety | Physical audio/input design suitable for guitar; no root modification or hidden network listeners |

MOD-UI reports CPU usage and XRUN counts. Tests must include an
actual signal source/device; simulated or desktop passes are **not**
physical acceptance evidence.

## Reversal

Back up referenced pedalboards, stop using either plugin, and manually
remove **only** `micrantha-delay.lv2` and `micrantha-tremolo.lv2`
under `/home/pistomp/data/.lv2/`; then refresh discovery when safe.
Never delete the whole user plugin directory or forcibly restart the
audio engine during a session.

Yocto is independently dispositioned by ADR 0008 (#27); do not block
ARM64 software integration on building another OS.
