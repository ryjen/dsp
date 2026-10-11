# Hothouse / Daisy hardware validation

This file records the hardware-only acceptance gate for issue #6.

Cross-compilation, native tests, and hosted CI **do not** satisfy unchecked items below. Each checked item must identify the physical target, firmware commit, and observed result.

## Test identity

- [ ] Board/carrier: Hothouse stereo revision recorded
- [ ] Daisy module revision recorded: Seed or Seed3
- [ ] Exact firmware commit SHA recorded
- [ ] Power/audio setup recorded

## Flash and boot

- [ ] Exact `hothouse_delay.bin` or `.elf` flashes successfully
- [ ] Firmware boots without reset loop or preparation failure
- [ ] Stereo audio passes through with expected channel ordering

## Runtime configuration

- [ ] Actual sample rate confirmed: expected 48,000 Hz
- [ ] Actual block size confirmed: expected 48 frames
- [ ] Callback period confirmed: expected approximately 1 ms
- [ ] CPU/headroom recorded under representative delay settings using `CpuLoadMeter` or equivalent out-of-callback reporting

Record:

```text
sample_rate_hz:
block_frames:
callback_period_us:
cpu_load_average:
cpu_load_peak:
measurement_method:
```

## Physical controls

- [ ] Pot 1 changes free delay time across the expected useful range
- [ ] Pot 2 changes feedback without instability
- [ ] Toggle 1 selects free / quarter / dotted-eighth subdivision
- [ ] Footswitch 1 bypasses/re-engages without an obvious discontinuity
- [ ] Delay tails/state remain continuous across bypass
- [ ] Footswitch 2 establishes tap tempo when MIDI is not authoritative
- [ ] Bounce/noise does not cause obvious repeated toggles or control chatter

## USB MIDI

- [ ] USB MIDI transport enumerates and receives messages
- [ ] MIDI Clock drives bounded tempo
- [ ] Start / Stop / Continue authority behavior matches the deterministic host tests
- [ ] channel-1 CC20 controls the primary/free-delay mapping
- [ ] channel-1 CC21 controls feedback
- [ ] unsupported channels/messages do not alter processor state
- [ ] tap tempo cannot override valid running MIDI clock
- [ ] Stop or clock timeout releases MIDI authority back to tap tempo

## Realtime behavior

- [ ] No audible dropout under representative delay settings
- [ ] Callback peak/headroom remains acceptable while controls/MIDI are changing
- [ ] Repeated bypass and control changes do not destabilize audio
- [ ] Long maximum-feedback decay remains finite
- [ ] Long decay does not show a callback-time spike attributable to subnormal/denormal state

## Denormal/subnormal disposition

Complete exactly one:

- [ ] Existing Daisy/runtime flush-to-zero behavior was measured/verified and documented
- [ ] Firmware explicitly enables a supported flush-to-zero policy and the setting is documented
- [ ] Narrow DSP suppression was required by evidence; implementation/ADR reference recorded

Evidence / notes:

```text
hardware:
firmware_sha:
test_duration:
observations:
cpu/headroom:
denormal_policy:
follow_up:
```

## Completion rule

Issue #6 may close only when every hardware-dependent acceptance item relevant to the selected target is checked with concrete evidence. If hardware differs from the expected 48 kHz / 48-frame configuration, record the discrepancy and update the target documentation rather than treating the compile-time configuration as observed fact.
