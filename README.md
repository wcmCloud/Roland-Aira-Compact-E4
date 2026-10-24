# E-4 Companion

JUCE C++17 starter for a Roland AIRA E-4 hardware companion: VST3, AU and Standalone on macOS. See [PLAN.md](PLAN.md) for capabilities, architecture, Ableton setup and phased work.

## Interface

The standalone and plug-in share a custom dark studio theme with lime accents, tinted momentary chord pads, rounded numeric controls and a full-width keyboard. Standalone groups device routing and clock in a sidebar; both versions keep performance controls and Panic prominent. Bend displays Centre or a percentage while retaining its existing MIDI range. Root labels use C4 (60) for middle C and accept MIDI numbers, sharp/flat names or their own formatted labels. The keyboard uses the same octave convention. Incoming MIDI highlights the original input notes independently of on-screen key presses, without retriggering output. The 49-note view follows incoming notes outside its current range; transpose does not change the displayed input pitch. A four-segment voice indicator flashes briefly for rejected notes, distinguishing the four-voice limit from out-of-range transposition. Connection status describes the opened MIDI route, not a hardware handshake.

Standalone defaults to 1040 × 780 and can resize; the plug-in is 960 × 650. Component-rendered previews are in [docs/ui](docs/ui). The incoming-note preview uses synthetic in-memory notes, not physical device traffic. To regenerate them without opening devices or reading/writing standalone preferences:

```sh
cmake --build build-check --target e4_ui_preview
./build-check/e4_ui_preview_artefacts/Debug/e4_ui_preview "$PWD/docs/ui"
```

## Documentation

[Roland E-4 owner’s manual (PDF)](docs/E-4_eng01_W.pdf) — local reference manual supplied for this project.

## Current behaviour

First implementation of the shared MIDI engine and plug-in interface:

- Maps incoming notes and pitch bend to the selected channel (default 3).
- Allows four distinct notes. A rejected fifth-note hold must be fully released before that same input pitch can be accepted again; this prevents a late release from cutting off a newer press. Other sources remain independent.
- Counts repeated notes and overlapping input-channel ownership so one release cannot silence another held source.
- Filters unsupported messages without discarding valid releases in the same block. Incoming clock/transport bytes are deliberately dropped: use Live's port Sync in plug-in workflows. Standalone has an optional internal clock master.
- Panic sends explicit Note Offs for owned notes and centres bend. Channel changes, host stop transitions, state restore and bypass also clean up owned notes.
- Transposes notes by −24 to +24 semitones; out-of-range notes are rejected rather than clamped. Changing transpose releases held output pitches and centres bend before new notes are accepted.
- Saves channel, chord root and transpose as schema-3 plug-in state; older schemas migrate to a default transpose of zero and, where needed, root 60. Held notes are never restored.
- Bounds incoming work to 2048 events per block, then releases owned notes and drops the remainder.

The plug-in emits silent audio; record the physical E-4 audio separately. Enable VOCODER, KEY IN and HARMONY on the hardware. VST3 host routing is the first supported design path, pending hardware testing. The dedicated standalone app now provides direct MIDI port management. The AU output workflow remains experimental: Ableton does not expose AU direct MIDI output.

Panic needs a processing callback to send MIDI; a suspended host, removed instance or disconnected device cannot be guaranteed a final cleanup message. The internal event staging buffer is preallocated, but the host-provided JUCE MIDI buffer may still grow when output is copied; strict no-allocation certification remains outstanding.

## Plug-in performance controls

The VST3/AU editor now includes an on-screen keyboard, momentary Major/Minor/Sus2/Maj7 chord pads, a root control, spring-centred pitch bend and an active-voice display and transpose. Host clips, the UI keyboard and each pad own their notes separately while sharing the physical four-voice limit. Channel, chord root and transpose can be automated; changing the root releases existing pad notes without replaying them.

Editor gestures enter a bounded single-producer/single-consumer queue. They are emitted at sample zero of the next processing block; clip MIDI retains its within-block sample offsets. These UI gestures are not sample-accurate automation. Lost queue events trigger note cleanup instead of leaving a dropped Note Off stuck. Closing the editor releases only UI-owned notes at the next callback, preserving notes owned by host clips. The bend is centred on editor close. Panic, bypass and host stop still release all owned notes.

In Live, load the VST3 on the controller track and route its MIDI output through a second MIDI track to the E-4 (see PLAN.md). The AU build has the same interface but its direct MIDI-output workflow remains unsupported. Plug-in UI controls require host processing callbacks to send anything; if the host suspends the instance, queued cleanup must wait.

## Standalone quick start

Open `build-check/AiraE4_artefacts/Debug/Standalone/E-4 Companion.app` after building `AiraE4_Standalone`.

1. Connect the E-4 and enable VOCODER, KEY IN and HARMONY on it.
2. Select its MIDI output and an optional keyboard/controller MIDI input. Press **Connect**; selecting a port alone does not change the active route. When connected, changed dropdown choices show Apply ports and keep the old route until you apply them.
3. Match the hardware MIDI channel (default 3). Play the on-screen keyboard or hold Major, Minor, Sus2 or Maj7 pads. Choose the chord root by MIDI note number or note name (for example C4 or Db4) and a transpose value in semitones. Keyboard, external notes and pads share the four-voice limit; additional distinct notes are rejected.
4. Drag pitch bend; it returns to centre when released. Double-click also centres it. Use Panic to release owned notes and reset bend.
5. Route/record audio from the E-4 separately. This app opens no audio device and synthesises no audio.

The app remembers desired input/output identifiers, channel, chord root and transpose, but never automatically reconnects or replays notes. Switching routes releases the old endpoint first; disconnect, channel/transpose change and shutdown also clear held notes. Device removal is polled roughly once a second. Sleep or a UI pause over two seconds disconnects until you explicitly reconnect. Internal clock defaults off and is never automatically restored. Incoming clock is not forwarded.

Incoming notes use a bounded queue drained on a 5 ms UI timer, so busy UI work can still delay input processing. Once generated, notes and bend enter a separate bounded queue consumed by the same dedicated sender as clock. Device send calls run exclusively on that worker; they do not block routine interface callbacks. Output overflow drops unsent presses, releases notes actually sent and disconnects until explicit reconnection. A missing physical endpoint may not receive cleanup, so device/hardware validation is still needed. Only one standalone app instance is allowed; ownership against other apps is not enforced.

## Standalone clock and transport

After connecting the output, enable **Internal clock (sole master)**, choose 20–300 BPM, and press **Start**. Start sends MIDI Start followed by the first clock pulse; subsequent clock pulses run at 24 PPQN on a dedicated high-priority thread. Tempo changes preserve the remaining fractional pulse phase. Stop sends MIDI Stop once, stops clock output, releases owned notes and centres bend. Panic also stops transport. MIDI clocks are sent only while this transport is running.

Clock starts off every launch and is disabled on route changes, disconnect and removal. Only BPM is remembered. Start after Stop is a fresh restart: no Continue or song-position chase is sent. Configure the E-4 clock source for USB or MIDI as appropriate; actual looper record/play arming and Start/Stop response require hardware verification. Do not enable the internal master alongside Live Sync or another clock sender. Incoming clock and transport are neither followed nor echoed.

The scheduler accumulates absolute deadlines; it does not add a period to each wake time. If a full pulse interval is missed, the sender sends Stop, releases its notes and waits for explicit reconnection instead of catching up in a burst. A two-second UI heartbeat timeout performs the same cleanup independently, even with clock off. All note, bend and clock device calls run on one sender thread. A slow device/driver can still delay that thread, and closing a route waits for it to finish cleanup. This is best-effort desktop scheduling, with no measured hardware jitter guarantee.

Software simulations at 60/120/180 BPM produce the expected pulse counts over ten minutes with under one microsecond of calculated deadline drift. Those results test the planner, not thread wake-up jitter, physical MIDI delivery or the E-4. Capture actual timing under load before using clock in a performance.

## Sender reliability

The standalone uses a 1024-command queue from the message thread to one MIDI sender. Normal note/bend/clock commands keep order, and the worker tracks the actual output pitches and channels it sent. Stop/Panic has a separate priority signal: it cancels pending presses, sends Stop if needed and releases sent notes, without closing a healthy route. Fresh input can resume after Panic.

Disconnect/shutdown discards unsent presses and performs final cleanup on the worker before destroying the endpoint. Queue overflow, a missed clock pulse or an overdue UI heartbeat performs fail-closed cleanup and requires explicit reconnection. A physically unavailable endpoint or permanently blocked OS send cannot be guaranteed to receive the final messages; hardware hotplug/load tests remain pending.

Portable tests exercise queue delivery, actual-sent-note cleanup, overflow, heartbeat loss, priority panic and 20,000 events crossing real producer/consumer threads. The sender suite passes AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer. These tests use fake endpoints and do not measure device latency.

## Debug launcher

From this project folder:

```sh
./scripts/run-debug.sh               # Build and open the Debug app
./scripts/run-debug.sh --debug       # Build and run under LLDB
./scripts/run-debug.sh --build-only  # Build without launching
```

The script uses the verified Makefiles build, avoiding the Xcode helper/cache problem encountered in automation. It finds the existing JUCE checkout or accepts `E4_JUCE_DIR=/absolute/path/to/JUCE`. It does not download dependencies. Build-only was verified; GUI launch and debugger attachment still require a local interactive check.

## MIDI diagnostics

Build the receive-only tool, then list ports:

```sh
cmake --build build-check --target e4_midi_probe
./build-check/e4_midi_probe_artefacts/Debug/e4_midi_probe --list
```

Use an **input** identifier from that listing to capture incoming MIDI for 1–600 seconds:

```sh
./build-check/e4_midi_probe_artefacts/Debug/e4_midi_probe --capture "INPUT_IDENTIFIER" --seconds 10 > capture.json
```

The tool never opens MIDI outputs. It reports note/bend/transport counts, clock arrival statistics and the first 256 short messages as JSON. Long SysEx payloads are not retained. Clock intervals reset across observed Start/Stop, so silence between sessions is excluded. Callback arrival statistics include OS/USB batching; they do not prove generated-clock accuracy. Run in your normal Terminal if a sandbox cannot reach CoreMIDI.

A five-second receive-only E-4 capture succeeded; see [the validation record](docs/validation/README.md). Outgoing notes, audio and Ableton behaviour remain unverified.

## Build

Prerequisites: Xcode command line tools, CMake 3.22+ and a JUCE checkout. These are now available on this Mac, with JUCE under `build/_deps/juce-src`. No additional dependency installation is needed for that checkout.

```sh
cd "/Users/chris/Documents/GitHub/Aira E4"
cmake -S . -B build -G Xcode -DJUCE_SOURCE_DIR="/absolute/path/to/JUCE"
cmake --build build --config Debug --target AiraE4_VST3 AiraE4_AU AiraE4_Standalone e4_core_tests e4_processor_tests e4_clock_tests e4_output_tests e4_vst3_check
ctest --test-dir build -C Debug --output-on-failure
```

Alternatively configure with `-DE4_FETCH_JUCE=ON` instead of JUCE_SOURCE_DIR to download the selected JUCE 8.0.10 baseline. Requires network access. Review JUCE licensing before distributing. Outputs are under `build/AiraE4_artefacts/Debug/`; nothing is automatically copied to system or user plug-in folders. Initial IDs are placeholders for development.

Core only, with CMake:

```sh
cmake -S . -B build-core -DE4_BUILD_APPS=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Core only, with the installed compiler:

```sh
mkdir -p build-core
clang++ -std=c++17 -Wall -Wextra -Werror -ISource Tests/CoreTests.cpp -o build-core/e4_core_tests
./build-core/e4_core_tests
```

## VST3 wrapper validation

The software-host check loads the built VST3 bundle without installing it or opening MIDI/audio hardware:

```sh
cmake --build build-check --target e4_vst3_check
ctest --test-dir build-check -R e4_vst3_wrapper_check --output-on-failure
```

It verifies note output/sample offsets across 44.1/48/96 kHz and 32/64/128/512/1024-sample blocks, silent audio, CC filtering, pitch bend, host transpose automation and state round-trip without note replay. [Saved results](docs/validation/vst3-wrapper-2026-10-24.json) pass all 24 checks. This JUCE-host test does not replace Ableton acceptance, Steinberg's validator, AU validation or physical hardware testing.

## Verification

Core tests cover ownership, limits, filtering/release interactions, channel cleanup, note/bend boundaries, panic and 1000 stress cycles. A second test target exercises the actual JUCE processor: sample offsets, filtering, state round-trip, invalid state, bypass and overload cleanup. Run all three with CTest after building `e4_core_tests`, `e4_processor_tests` and `e4_clock_tests`.

VST3, AU and standalone Debug builds pass in `build-check`; all seven CTest checks pass (core, clock, sender, processor, VST3 wrapper, diagnostic help and invalid-argument checks). The built app is `build-check/AiraE4_artefacts/Debug/Standalone/E-4 Companion.app`.

Core tests also pass with AddressSanitizer and UndefinedBehaviorSanitizer on Apple clang 21, arm64. Read-only E-4 discovery and incoming clock capture passed. Outgoing hardware and DAW integration remain untested. See PLAN.md for the current milestone status.

The automated Xcode build encountered a module-cache write error in this session; a later Xcode refresh also failed during JUCE helper compiler discovery. The successful verification used a separate Makefiles build, preserving the existing Xcode project. To refresh Xcode from your own Terminal, run the CMake configuration command above with the local JUCE path. The already built standalone app in `build-check` is available independently of that refresh.

Manual acceptance steps are in [docs/HARDWARE_TESTS.md](docs/HARDWARE_TESTS.md). These checks have not yet been performed on an E-4 or in Ableton.

## Performance expansion

The first foundation increment adds shared chord settings, validated versioned preset data and independent ownership for future note generators. Factory performance remains unchanged; editable banks and import/export controls are next. See [the implementation roadmap](PLAN.md) and [the shared performance contract](docs/architecture/PERFORMANCE.md).
