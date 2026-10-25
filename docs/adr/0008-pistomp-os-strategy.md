# ADR-0008: Retain upstream pi-Stomp OS instead of building Yocto firmware

- **Status:** Accepted — no-go for a custom Yocto image at this stage
- **Date:** 2026-10-25
- **Issues:** #26, #27

## Explain it to a human

Our C++ guitar effects are already portable LV2 plugins. pi-Stomp supplies
an actively maintained audio-ready Linux image with hardware drivers,
MOD-UI and an update mechanism. Rebuilding the whole distribution would
add considerable maintenance work before proving a benefit.

## Verified upstream starting point

The current pi-Stomp image is a Debian Trixie **arm64** Raspberry Pi OS
derivative with RT kernel, JACK2, mod-host, MOD-UI and pi-Stomp controller.
It is maintained via `TreeFallSound/pi-gen-pistomp` and apt releases.
Current user plugins live in `/home/pistomp/data/.lv2`, while factory
plugins live in `/usr/lib/lv2`. v3 generally uses Pi 5; v2 uses Pi 3/4.
Board/image variations must be checked before deployment.

Primary references:
- https://treefallsound.github.io/pistomp-manual/developers/building-images/
- https://treefallsound.github.io/pistomp-manual/using/mod-ui/
- https://github.com/TreeFallSound/pi-gen-pistomp
- https://github.com/TreeFallSound/pi-stomp

## Alternatives and trade-offs

| Direction | Advantage | Additional ownership | Decision |
| --- | --- | --- | --- |
| Official pi-Stomp OS + ARM64 LV2 plugins | Maintained audio stack, hardware integration, MOD-UI, updates | Version drift and physical acceptance still require evidence | **Adopt** |
| Custom Yocto + meta-raspberrypi | Controlled rootfs, packages, provenance, boot/update policy | BSP/codec drivers, JACK/mod-host/MOD-UI/controller recipes, kernel tuning, CVEs, OTA/rollback, support | **No-go without requirement** |
| Elk Audio OS (Yocto/OpenEmbedded reference) | Demonstrates embedded realtime Linux and alternative audio architecture | Uses different hosts/drivers; not a drop-in pi-Stomp replacement | **Study only** |

## Yocto feasibility inventory, if reopened

Before approving Yocto work:
1. Select a currently supported Yocto LTS and corresponding
   `meta-raspberrypi` machine branch; pin `kas` repos to verified commits.
2. Validate Pi bootloader, DT overlays, audio codec/ADC/DAC,
   guitar-input impedance and GPIO/I2C/SPI drivers on selected hardware.
3. Package JACK2/LV2/lilv, mod-host, MOD-UI and the pi-Stomp controller
   with the required service/users/permissions, without source-tree coupling.
4. Design read-only root vs persistent presets, signed A/B OTA update and
   recovery, watchdog, SBOM, vulnerability/backport process, and all
   software license obligations (including AGPL).
5. Compare end-to-end latency, XRUNs, resource usage and reliability on
   identical **physical** hardware, with measurable acceptance thresholds.
6. Budget image build resources, caching, release and ongoing maintenance.

Neither a `kas` build nor a QEMU/physical Yocto image was produced in this
**negative go/no-go evaluation**. Doing so would be implementation before
a justified decision, not necessary evidence to decline the work.

## Reconsider only if

- The upstream image demonstrably cannot meet a concrete security, signed
  update, lifecycle or supply-chain requirement of a planned product.
- The official audio stack fails measured performance/latency requirements
  and the deficiency requires BSP/kernel work, rather than normal tuning.
- A separate educational Yocto/QEMU experiment is explicitly prioritized;
  it should be scoped independently of pi-Stomp delivery.

## Outcome

Use the current official pi-Stomp image, build plugins **natively against the
device's Debian arm64 runtime**, validate MOD-UI and physical audio under #26,
and keep Daisy/Hothouse firmware unaffected. Close #27 as **not planned**;
do not treat unbuilt Yocto images as completed acceptance criteria.
