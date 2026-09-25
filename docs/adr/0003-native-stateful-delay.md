# ADR-0003: Native stateful delay

- **Status:** Accepted
- **Date:** 2026-09-24
- **Issue:** #5
- **Related:** ADR-0001, ADR-0002

## Context

ADR-0002 adopts Faust selectively when generated signal-flow code improves clarity, portability, or testability. Delay is the second portable effect slice and has a different architectural purpose: prove explicit prepared memory ownership, circular-buffer bounds, feedback stability, fractional reads, and live control transitions.

The effect must run through the same host-independent `dsp::Processor` boundary as tremolo without depending on MIDI packet formats, JUCE, Daisy, filesystem APIs, or network APIs.
## Decision

Implement `DelayProcessor` in native C++.

Preparation owns all variable-sized memory. Each prepared audio channel receives an independent circular buffer sized to `ceil(sample_rate * 2.0) + 2` samples. The callback never grows or reallocates that storage.

The public normalized controls are:

- free delay time: 1–2000 ms;
- feedback: 0–0.95;
- wet/dry mix: 0–1;
- normalized tempo: 20–300 BPM;
- subdivision: free, quarter, dotted eighth, eighth, eighth triplet, or sixteenth.

MIDI clock and packet decoding are not effect responsibilities. A future realtime-control adapter may derive normalized BPM or scalar targets and publish them through this interface.
## Delay-line algorithm

For every frame, the processor advances one shared delay-time smoother, feedback smoother, and mix smoother. Each channel then reads its own ring buffer at the shared fractional delay position, writes `input + delayed * feedback`, and outputs the wet/dry blend.

Fractional positions use linear interpolation between adjacent ring samples. Delay samples are always clamped to the prepared range `[1, capacity - 2]`, avoiding the currently written slot and keeping interpolation inside allocated state.

Channels share timing/control state and the write index, but feedback audio remains channel-local. Ping-pong and cross-channel feedback are outside this slice.
## Live transitions

Initial `prepare()` and `reset()` apply the currently published controls directly so deterministic impulse placement begins from the first sample.

Subsequent delay-time target changes ramp for 100 ms. This includes changes caused by free-time updates, BPM changes, or subdivision changes. Fractional interpolation during the ramp deliberately yields tape-style pitch glide instead of an instantaneous read-head jump.

Feedback and wet/dry changes ramp for 10 ms. A target that has not changed does not restart its smoother.

Dual-head crossfading is deferred. It can remove tape-style pitch glide, but it adds a second read path and transition state that are unnecessary until listening or hardware evidence shows that behavior is preferable.
## Realtime and safety bounds

After successful preparation:

- `process()` performs work proportional only to frames × channels;
- no dynamic allocation, filesystem/network I/O, logging, or blocking lock occurs in the callback;
- malformed blocks that exceed prepared frame/channel bounds are ignored;
- null channel pointers are skipped;
- non-finite scalar setter values are ignored;
- legal scalar inputs are clamped before publication;
- maximum feedback remains below unity at 0.95.

The realtime allocation test tracks global allocation calls across 2,000 prepared callbacks and fails on any allocation.
## Evidence

Deterministic tests cover integer and fractional impulse placement, known feedback decay, wet/dry extremes, free-time and normalized-control clamping, re-prepare state clearing, null/oversized block handling, musical subdivision resolution, live delay/tempo/mix transitions, silence, and bounded finite DC behavior at maximum feedback.

The callback-budget harness executes representative 48 kHz / 64-frame stereo and 96 kHz / 128-frame stereo configurations. Timing values are evidence rather than machine-specific pass/fail thresholds.

The same `dsp::Processor` boundary now hosts both a selectively generated Faust tremolo and a native stateful delay, demonstrating that effect implementation technology does not define the repository ABI.
## Consequences

Benefits:

- explicit and auditable ownership of stateful realtime memory;
- no generator dependency for a state-heavy effect where native code is clearer;
- normalized tempo synchronization without coupling DSP to transport protocols;
- a reusable pattern for future bounded circular-buffer effects.

Costs:

- hand-written interpolation and ring-buffer bounds remain project responsibility;
- tape-style pitch glide is an intentional first implementation trade-off;
- changing the two-second maximum requires re-preparation and memory sizing changes.

## Alternatives rejected

**Faust delay kernel:** valid technically, but rejected for this slice because explicit prepared memory ownership and native stateful-effect conventions are the primary evidence.

**Instant delay-time jumps:** rejected because read-head discontinuities can click.

**Dynamic callback-time ring growth:** rejected because it violates realtime allocation and bounded-work requirements.