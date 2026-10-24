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

## Scale feature validation

The scale milestone passes all seven CTest checks and the core suite under AddressSanitizer/UndefinedBehaviorSanitizer. Exhaustive key/scale/mode tests use an independent degree-list oracle across MIDI 0–127. Ownership regressions cover collision releases, repeated/channel holds, pad opt-in, scale changes and transpose order. The actual VST3 wrapper passes 28 checks, including scale automation, mapped note/release sample offsets, reject mode and state recall; see [the scale wrapper report](vst3-scales-2026-10-24.json). No hardware MIDI output or audio device was opened by these checks; Live and physical E-4 acceptance remain open.

## Controller mappings and current Live/hardware checks

Eight CTest suites pass, including the new controller-router suite. Mapping and
core suites pass AddressSanitizer/UndefinedBehaviorSanitizer; the shared exchange
concurrency regression also passes ThreadSanitizer. The actual VST3 passes 31
checks, adding controller-profile state loading, consumed pad trigger/release
sample offsets and CC20-to-transpose delivery in a JUCE host. See
[vst3-mappings-2026-10-24.json](vst3-mappings-2026-10-24.json).

The development VST3 is installed in the user VST3 folder. Live 12.4.7 Lite
scanned/loaded it; a generated-MIDI send was configured to E-4 channel 3 and
saved as the project-local routing set. After installing the final build, Live
was restarted and the saved route reopened successfully. Software MIDI traffic
was seen for a short computer-keyboard note; audible hardware receipt is not
confirmed. See [the Live routing record](../ableton/README.md).

E-4 MIDI endpoints are present in
[midi-ports-mappings-2026-10-24.json](midi-ports-mappings-2026-10-24.json).
A fresh five-second receive-only capture is saved in
[e4-receive-mappings-2026-10-24.json](e4-receive-mappings-2026-10-24.json);
its timing is callback arrival timing, not outgoing jitter or an audio result.
The Mac reports a two-input/two-output USB E-4 audio device at 44.1 kHz in
[audio-devices-mappings-2026-10-24.txt](audio-devices-mappings-2026-10-24.txt).
Live repeatedly failed to open that audio input with “Selected audio device is
not available,” so audio return remains unverified, not passed. Its input was
restored to the working Mac microphone and system output, and the return track
remains Monitor Off. No microphone recording was captured.

Physical-controller learn/CC receipt in Live, audible four-voice/bend playback,
Panic/stop/hotplug/sleep on hardware and AU output acceptance remain open.
