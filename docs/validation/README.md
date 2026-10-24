# Local MIDI validation — 24 October 2026

Environment: macOS 27.0.1, arm64, JUCE 8.0.10 Debug diagnostic build.

## Verified

- CoreMIDI discovery returned E-4 MIDI OUT (input to the Mac) and E-4 MIDI IN (output from the Mac).
- A receive-only capture on E-4 MIDI OUT ran for 5.005 seconds.
- Captured 260 clock messages and 259 positive intervals; no notes, bend, Start or Stop were observed.
- Mean callback interval: 19.231008 ms, corresponding to approximately 129.998 BPM at 24 PPQN.
- Observed callback interval range: 18.869583–20.108458 ms; sample standard deviation: 0.430179 ms.

These are local receive-callback arrival times. They include USB/CoreMIDI/OS scheduling effects and do not measure the standalone application's generated clock, hardware transmission timestamps or round-trip latency. No MIDI output port was opened and no messages were sent. Firmware, vocoder setup, outgoing notes, audio return, looper transport and Ableton routing remain unverified.

The first sandboxed discovery attempt returned no endpoints because CoreMIDI access was restricted. Running with access to the Mac MIDI service succeeded; empty arrays alone do not prove the E-4 is disconnected.

Raw records: [port inventory](midi-ports-2026-10-24.json) and [five-second receive capture](e4-receive-2026-10-24.json). IDs are local endpoint identifiers and can change after reconnecting/reconfiguration.

## VST3 software-host validation

The actual Debug VST3 was discovered and loaded in a JUCE software host. All 24 checks passed, including note sample offsets across 15 sample-rate/buffer combinations, CC filtering, bend, host transpose automation and state round-trip without note replay. See [the wrapper report](vst3-wrapper-2026-10-24.json).

This test opened no audio or MIDI device and transmitted no hardware messages. It is not a substitute for Ableton, the Steinberg validator or AU validation.

## Dedicated sender validation

All seven CTest checks pass after moving standalone device writes onto one worker. The new portable sender suite covers queued delivery, channel/pitch-aware cleanup, priority Panic, shutdown cancellation, output overflow, clock deadline faults, UI heartbeat loss and 20,000 events crossing separate producer/consumer threads. It also passes AddressSanitizer/UndefinedBehaviorSanitizer and a separate ThreadSanitizer run on this Mac.

These checks use fake output endpoints. They establish software ordering and failure handling, not physical USB/DIN timing or driver hotplug behaviour.
