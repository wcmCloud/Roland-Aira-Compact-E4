# E-4 Companion — implementation plan

Status: standalone internal clock and transport implemented, 24 October 2026. Target: macOS VST3, AU and standalone using JUCE/C++17. Hardware/host validation is still pending.

## Implementation progress

- Fixed missing external-keyboard/host-clip highlights. Incoming-note display state is tracked separately by input channel and repeated holds, then painted alongside mouse/computer-keyboard state. Visual updates never call note listeners or generate MIDI. The view follows out-of-range incoming pitches; highlights show input pitches before transpose.
- Added reset and overlap tests, a processor-level publication check and native component checks proving display-only updates cannot retrigger output or release a mouse-held key. The preview tool now includes a synthetic incoming-chord image. Actual device timing/performance acceptance remains separate.

- Standalone outgoing notes, bend, clock and transport now share one device worker. Message-thread callbacks post to a bounded 1024-command SPSC queue instead of calling the MIDI output API or waiting on a port lock. Incoming MIDI/domain processing still uses the existing 5 ms UI drain; measured latency remains open.
- Sender tracks actual sent notes/channels for cleanup. Priority Panic cancels pending presses without closing a healthy route. Shutdown discards unsent presses and releases only sent notes before endpoint destruction. Overflow, clock deadline or UI-heartbeat faults clean up and require explicit reconnection, preventing stale-event replay.
- Added a portable sender test suite with fake endpoints, failure/cleanup cases and a real two-thread 20,000-event ordering stress test. All seven CTest checks pass; sender tests also pass AddressSanitizer/UndefinedBehaviorSanitizer and ThreadSanitizer. Physical driver timing and hotplug acceptance remain pending.

- Added consistent musical labels (middle C = C4 / MIDI 60), sharp/flat/numeric root entry and four-segment voice indicators. Engine rejection telemetry is published to the UI without locks; the display distinguishes voice limit, out-of-range notes and counter-reset conditions. Regression tests cover all 128 label round-trips, enharmonics, invalid input and processor feedback.
- Port selection now distinguishes staged choices from the active route, offers Apply ports when connected and disables unavailable connection actions. Applying still performs the existing old-route cleanup; selecting alone sends no MIDI.

- Redesigned both interfaces with a shared native JUCE theme: dark panels, lime/teal/lavender/amber pad accents, rounded controls, a full-width keyboard and clearer connection/voice/transport status. Standalone now groups routing and clock in a sidebar; plug-in controls follow the same visual hierarchy. The MIDI engine, IDs and state schema are unchanged.
- Added a UI rendering tool that exports the real standalone and plug-in components without opening MIDI devices or loading/saving standalone preferences. Both default-size snapshots have been visually inspected. Live mouse/keyboard and window-resizing acceptance remain separate manual checks.

- Hardened rejected-note bookkeeping: repeated presses of a rejected fifth voice remain blocked until that input's outstanding releases drain. Source cleanup clears rejected history, and counter saturation triggers panic rather than losing ownership. Core/processor regressions reproduce and verify the fix.
- Added VST3 software-host validation that discovers and loads the actual bundle, checks notes at 15 sample-rate/buffer combinations, pitch bend, CC filtering, automation and state round-trip. All 24 wrapper checks and all six CTest checks pass. Results are saved under docs/validation; Ableton, Steinberg/AU validation and outgoing hardware acceptance remain pending.
- Fixed UTF-8 decoding for the bullet/dash labels in both interfaces; all formats rebuilt.

- Added receive-only MIDI diagnostics (JSON port inventory and bounded input captures with online clock statistics), plus a Debug launcher that can build/open the app or run under LLDB. Build-only is verified; interactive debugger attachment remains pending.
- Physical receive-path check on 24 October: discovered E-4 MIDI IN/OUT and captured 260 incoming clock pulses over five seconds at approximately 130 BPM. Raw results and limitations are saved under docs/validation. No MIDI output was opened or messages sent; outgoing notes, looper sync, audio return and Ableton acceptance remain open.

- Standalone now has an opt-in 20–300 BPM internal clock master and Start/Stop controls. MIDI Start/Clock/Stop are sent through the selected port; clock runs on a dedicated high-priority thread with absolute deadlines and tempo phase preservation. No Continue, song position or incoming-clock follow is implemented.
- Clock defaults off and never auto-restores. Panic, Stop, route/disconnect and shutdown stop transport; missed pulse intervals stop instead of producing catch-up bursts. A two-second UI heartbeat timeout also stops output and releases sent notes. One sender worker owns all device writes; normal UI callbacks enqueue rather than send. Real hardware delivery may still block, and timing guarantees remain unmeasured.
- Three test suites pass, including deterministic ten-minute 60/120/180 BPM simulations, maximum tempo, Start/Stop ordering, tempo phase changes, missed-deadline failure and explicit restart. Phase 4 is partial: physical jitter/drift captures, looper response and external-follow modes remain open.

- Added shared −24…+24 semitone transpose to standalone and plug-in, including host automation. Out-of-range output pitches are dropped, not clamped. Changing transpose releases old pitches, clears pending UI performance and centres bend. Source-specific release also maps back to the actual output pitch.
- Plug-in state is now schema 3 with migration from schemas 1 and 2; older sessions default to zero transpose. Standalone persists transpose independently. Added range-sweep and processor regressions for original-pitch cleanup, host sample offsets, chord/editor cleanup and migration.
- Added docs/HARDWARE_TESTS.md as a repeatable manual acceptance record; all hardware/host results remain pending.

- Plug-in editor now has keyboard, four chord pads, root selection, spring-centred bend and active-voice display. GUI gestures cross a bounded SPSC queue; processing never locks the GUI. Queue overflow cleans up notes. Host events keep their sample offsets, while UI events land at the next block boundary.
- Editor-close cleanup releases sources 1–5 and preserves host source 0, including overlapping pitches. Chord-root changes release original pad notes. Schema 3 persists transpose/root/channel and migrates schemas 1 and 2 without replaying held notes.
- Added processor regressions for pad press/release, host/UI overlap, editor-close signalling, pending gestures, root automation, bend, UI queue overflow and state migration/rejection. Native visual inspection and Ableton/hardware acceptance remain open.

- Dedicated MIDI-only standalone app replaces the stock JUCE wrapper. Added input/output selectors, explicit connect/disconnect, on-screen keyboard, spring-centred bend and active-voice display. It reuses the shared MIDI engine without requiring an audio device.
- Added four momentary chord pads (Major, Minor, Sus2, Maj7) with a selectable root. Pad note snapshots and independent ownership protect external/keyboard notes during releases. All sources share the four-voice limit.
- Desired endpoint IDs, channel and chord root persist. Outputs never auto-connect. Route changes release the old port before closing; unplug detection and long UI pauses require explicit reconnection. Shutdown/panic clear held notes. App instance duplication is disabled, but cross-app endpoint arbitration is not implemented.
- Standalone notes use a bounded callback queue and 5 ms UI drain; internal clock uses a dedicated thread. Incoming transport following and latency/jitter certification remain open. Device presence is polled approximately once a second; hardware cleanup cannot be guaranteed after physical removal.
- Regression coverage now includes fake-endpoint route switching/disconnect, no replay, keyboard/external ownership, queue overflow/recovery and chord snapshot/overlap release. Earlier native app automation timed out; offline component snapshots are now visually verified. Live interaction and real hardware workflow remain unverified.

- Shared fixed-storage MIDI engine is integrated with the JUCE processor. Implemented: output channel, four distinct voices, repeated-note/input-channel ownership, velocity-zero releases, raw pitch bend, unsupported-message filtering, owned-note panic and channel-change cleanup.
- Plug-in state saves channel, chord root and transpose. MIDI bend and note gestures are transient, not restored.
- Host stop transitions and bypass release notes. Input bursts are bounded at 2048 events/block; overload triggers cleanup. Plug-in clock is intentionally delegated to Live port Sync. Standalone can generate an opt-in internal clock master; incoming clocks are not forwarded.
- Verification: VST3, AU and standalone Debug builds pass on arm64 in `build-check`; all three CTest suites pass. Core tests also pass under AddressSanitizer/UndefinedBehaviorSanitizer. JUCE VST3 discovery/load and processing checks pass. Ableton scan and outgoing hardware tests remain pending; read-only discovery and incoming clock capture passed.
- Core regression tests and processor integration tests have been added. Full hardware test M1 remains open; M2 now has keyboard/chord performance controls in both interfaces. Preset/chord customisation and hardware/host acceptance remain open. M3 has its first MIDI-only workflow; physical hotplug, sleep and endpoint tests remain open.
- Xcode build/refresh remains unverified in this automated session: module-cache writing and JUCE helper compiler discovery failed. The separate Makefiles build succeeds; existing Xcode project files were preserved.
- CMake and JUCE are now installed/available locally. The CMake project now explicitly enables C as well as C++ for JUCE's VST3 helper.
- Strict real-time certification remains open: the engine and internal staging use bounded storage, but copying into host-owned MidiBuffer can require capacity growth. Panic depends on receiving another callback; removal, suspend or disconnect may prevent cleanup.



## Product goal and scope

Make playing and synchronising the physical Roland AIRA E-4 convenient from Ableton Live or a standalone app. This is a hardware companion, not a software reproduction of the E-4 effects. The hardware supplies the vocoder/effects and audio. Start with channel selection, notes/chords, pitch bend, reliable note cleanup and clear routing instructions. Add clock only after the note path works.

### Verified v1.02 capability boundary

| Message or feature | Hardware capability | Product consequence |
| --- | --- | --- |
| Note On/Off | Receives note numbers 0–127 | Play vocoder carrier pitches; handle velocity-zero Note On as release |
| Chords | Up to four voices with HARMONY enabled | Four-note app allocation policy; verify fifth-note behaviour rather than assume hardware stealing |
| Pitch Bend | Receives bend | Expose raw 14-bit bend, centre 8192; measure musical range and response on hardware |
| MIDI channel | 1–16, default 3 | App selection must match manually configured hardware channel |
| Timing Clock, Start, Stop | Transmits and receives | Sync is possible; receiving Start/Stop does not imply arbitrary looper editing |
| Continue, song position | Unsupported | No song-position chase; define explicit restart behaviour after seek/resume |
| CC, SysEx, program change, aftertouch | Unsupported | No parameter editor, preset dump/restore, firmware tools or automatic device identification |
| All Notes Off / All Sound Off | Not recognised | Panic sends explicit Note Offs, not CC 123 or CC 120 |
| Active sensing | Recognised/transmitted | Do not emit in MVP; characterise timeout before supporting it |

The app cannot remotely turn VOCODER/HARMONY/KEY IN on, set formant/reverb/scatter/waveform, or operate looper record/overdub/clear/undo via documented parameter messages. Save app settings/chords only; do not label those files as hardware presets. MIDI clock and transport can synchronise the looper, but record/play scheduling and actual Start/Stop behaviour need a hardware test. Do not infer complete two-way hardware state from MIDI traffic. This baseline intentionally targets the v1.02 manual even if newer firmware exists.

Hardware preparation: enable VOCODER and MIDI KEY IN, match channel (initially 3), and enable HARMONY for four voices. For clock, select USB or MIDI as the appropriate sync source; SYNC IN overrides MIDI clock when connected. Keep these instructions visible in onboarding.

## Architecture decisions

### Shared C++ domain core

Keep allocation, note ownership, filtering, chord generation, pitch bend encoding, transport state and state-schema logic independent of JUCE UI and physical ports. Fixed-capacity data structures and preallocated event storage belong on real-time paths. Preserve sample offsets and order, especially note releases before replacements. Track notes actually sent, including their original channel; changing channel/transpose/route must release the old notes first. Overlapping live and generated notes need reference counts so one source cannot silence another. On capacity overflow prioritise releases and enter a recoverable error state rather than silently losing them.

### VST3: first Ableton milestone

Use a JUCE instrument-style shell with silent stereo audio output and MIDI input/output. This makes it discoverable on Live MIDI tracks while it generates no software sound. Receive host notes and automation; emit supported MIDI events via the host. Host owns device routing in the default mode. Live can alter channel information between tracks; set the final E-4 channel at the hardware-output track. VST3 MIDI and parameter representations differ: specifically test pitch bend through the wrapper and host. Raw system real-time bytes must not be assumed to survive VST3 routing. Use Live's port Sync for the first release.

Do not open physical MIDI ports in processBlock. An optional direct-device mode is a later feasibility spike: it needs a timestamped bounded queue, a dedicated sender, hotplug handling, explicit port ownership and only one active clock source. Offline export must never send to physical hardware. Multiple instances require ownership arbitration before direct output can ship.

### Standalone: full device workflow

The standalone is a dedicated MIDI-only JUCE application using the shared MIDI engine. It provides input/output selectors, remembered device identifiers, explicit reconnection, keyboard/chord controls and internal clock. It opens no audio device; optional monitoring remains future work. Port names alone are not unique, so identifiers are retained and connections remain explicit. Route changes release the old endpoint first; held notes never replay on reconnection.

Standalone can send notes/bend/clock/start/stop directly to USB MIDI or a compatible DIN MIDI interface. A scheduler thread owns timed output; UI timers are unsuitable for clock. Expose local tempo or an incoming clock source, never both. If used beside Live, choose Live as clock master and disable standalone clock transmission. A virtual MIDI port may bridge the DAW to standalone, but must exclude echo routes.

### AU: limited host route, explicit release gate

Build an AU shell, but do not promise VST3-equivalent MIDI output in Live. Ableton documents that AU direct MIDI output is unavailable. Evaluate AU as an automation/control front end to the standalone service or a deliberately selected direct-device backend. Either choice adds connection lifecycle and multi-instance complexity. Until proven, mark AU as an experimental shell, keep VST3 recommended in Ableton, and do not market AU as fully functional. Generic JUCE Standalone is not evidence this AU transport works.

### UI and persistence

Start with port/routing status, channel, note keyboard, up-to-four-note chord pad, bend with spring-to-centre, panic, clock-owner indicator and manual hardware setup checklist. Disable unavailable hardware controls rather than fabricate mappings. Persist versioned app state: channel, chord slots, bend configuration, desired endpoint and clock mode. Do not restore held notes or playing transport on load. Hardware state is unknown unless the user confirms it; connection presence is not device identity.

### Audio

Keep the MIDI control path separate from audio return. Receive E-4 output through a selected interface or verified USB audio route. Do not assume that any USB audio stream feeds the effect input: consult the USB OUT ROUTING setting, and test the actual signal path. Avoid simultaneous dry monitoring and a returned signal that creates feedback. The initial plug-in clears its output buffer and does no audio processing or audio return. A later audio pass-through variant would need an explicit bus/layout and routing design.

## Ableton Live setup and acceptance recipe

1. Connect E-4 USB MIDI or DIN through an interface; prepare VOCODER/KEY IN/HARMONY and the matching hardware channel. Confirm the output endpoint name in Live Preferences/Settings.
2. Enable Track on that MIDI output. Enable Sync only if Live is the chosen clock master; start with other clock senders disabled.
3. Create a MIDI track called E-4 Controller, load the VST3, and feed a clip or keyboard into it. Its audio output is intentionally silent.
4. Create a second MIDI track called E-4 Send. Select E-4 Controller in MIDI From, then select the VST plug-in in the lower chooser; set Monitor to In. Set MIDI To to the E-4 hardware endpoint and final channel (default 3). Do not route this track back to the controller.
5. Create an audio return track, choose the actual E-4/interface input, and monitor at a safe level. If the installed Live edition offers External Instrument it may combine the send/return; this is optional. The Mac has Live 12 Lite, so the two-track MIDI plus audio-return route is the baseline.
6. For sync, use Live's hardware-port Sync and adjust MIDI Clock Sync Delay against a recorded return. Keep app-generated clock off. Test start, stop and loop wrap; set restart expectations for resume/seek because Continue and song position are unsupported.
7. Save a tested Live set only after recording a single note, four-note chord, bend sweep and stop/panic. Document actual MIDI endpoint/channel and audio route. Hardware processing requires real-time recording; offline rendering cannot recreate it.

Use no AU MIDI-output route in Live until the separate AU backend milestone passes. Internal Live track routing can merge MIDI channels, so verify the channel at the final output using a MIDI monitor.

## Phases, deliverables and exit gates

### Phase 0 — baseline and dependency setup (scaffold delivered)

Deliver CMake targets, processor shell, standalone wrapper, capability header, core test and this plan. Preserve existing content; initial folder was empty and not a Git repository. Xcode/Apple clang 21 arm64 and Homebrew are present; CMake and JUCE were not found in the locations checked. A JUCE 8.0.10 tag is selected as an explicit baseline, not asserted to be latest. Prefer a local checkout; downloads require E4_FETCH_JUCE=ON. Review JUCE licensing before distribution. Placeholder bundle/manufacturer/plugin IDs must be settled before publishing or saving compatibility-sensitive sessions.

Exit: install/provide CMake 3.22+ and JUCE; configure and compile VST3, AU and Standalone; launch standalone and scan plug-ins in a host. Current validation only covers dependency-free core compilation; no JUCE binary or host scan has been performed.

### Phase 1 — hardware and host feasibility (first priority)

Build a minimal route, use an independent MIDI monitor, and record the device firmware/channel/audio endpoints. Verify notes 0/127 and musically usable ranges, velocity handling, four voices with HARMONY, bend direction/range/centre and note-release behaviour. Probe Start/Stop and clock without changing undocumented device parameters. Test both USB and DIN if both are intended release paths. Confirm VST3 bend and note output in installed Live; explicitly record AU output failure/limitations and decide its backend.

Exit milestone M1: reliable four-note performance and recorded audio return in Live, with a repeatable routing recipe and a capability report. Block deeper UI work on a proven VST3-to-hardware path.

### Phase 2 — shared event engine and performance UI

Implement filtering, channel mapping, four-voice allocator, explicit steal/retrigger rules, keyboard and chord pads, bend, panic and versioned state. Suggested initial policy: reject fifth distinct held note with visible feedback; offer oldest-note stealing only after ownership/release semantics are tested. For repeated same-note input, count holds and issue the final release once all owners release. Panic enumerates owned notes; offer a separate full-channel sweep of 128 individual Note Offs plus bend centre. No CC fallback.

Exit M2: no stranded notes across repeated notes, transposition/channel changes, stop/start, bypass, state restore or editor closure. Host bypass and instance removal do not always allow a final callback: define user recovery via panic and test real host behaviour. Normal audio callbacks allocate no memory, block no locks and access no device APIs. Replace current transparent MIDI shell with tested policy integration.

### Phase 3 — standalone device management

Build dedicated app/device controller, port selectors, connection state, MIDI input callback queues, bounded timestamped output and explicit panic/reconnect behaviour. Add error UI for missing/ambiguous devices and settings persistence. Avoid feedback loops and handle device busy errors. Test app sleep/wake, USB removal and reopening, sample-rate changes and shutdown with held notes. Audio monitoring is optional and must have a clear mute control.

Exit M3: standalone notes, four-voice chords and bend work without Live; unplug/replug neither crashes nor replays old notes. No device access occurs on the audio thread. Direct MIDI remains opt-in and routes are visible.

### Phase 4 — clock/transport

Internal standalone 24 PPQN scheduling now uses accumulated monotonic-time deadlines. Fractional-sample scheduling remains relevant only if a future host-clock mode is added. Test tempo changes, nonzero starts, stop/start edges, loop wraps, tempo ramps, missing playhead data and offline host processing. Use host PPQ/play state for planning but do not route raw clock through VST3 unless demonstrated to work. Default VST3 path remains Live Sync. Standalone has an internal clock; external-follow remains future work. Maintain explicit ownership and no clock echo. Continue is not sent; after resume/seek either require user restart or send a documented fresh Start, without claiming song-position alignment.

Exit M4: capture 60/120/180 BPM for ten minutes each, report mean rate, worst interval error, jitter distribution and drift under CPU/UI load; provisional target is mean tempo error under 0.1% and drift under one pulse over ten minutes. Record measured jitter before setting a supported performance claim. Validate looper start/stop by listening and recording, including manual arming requirements.

### Phase 5 — host integration, AU decision and polish

Automate app-owned musical parameters (chord slot, transpose, bend) only. Decide and implement AU companion/direct backend or clearly ship it as experimental and exclude it from the advertised feature set. For service mode define versioned IPC, discovery, connection loss, one sender per endpoint and cancellation of queued events. Test multiple instances and duplicate clock prevention. Add scalable accessible UI, onboarding and routing templates. Restore app state without firing notes.

Exit M5: VST3 project save/reopen is repeatable in Live; AU passes its declared workflow in an AU host; standalone has consistent state migration. All formats clearly disclose their supported routes.

### Phase 6 — release

Build native arm64 first; add x86_64/universal only after validation. Run AU validation (auval), a VST3 validator and host scan, then sign/notarise and test installation/removal on a clean account. Keep copying/installing plug-ins explicit; CMake does not install into user plug-in folders automatically. Confirm final IDs/version policy and JUCE licensing. Include quick start, supported firmware/hosts, real-time recording guidance and limitations.

Exit M6: signed packages, recorded hardware regression results and a reproducible release checklist. No claim of a knob editor, E-4 emulation, hardware preset backup or guaranteed sample-accurate external hardware timing.

## Test strategy

- Core: note-on velocity zero, note boundaries, channel mapping, overlapping ownership, four-note limit, fifth-note decision, stealing/releasing order, bend endpoints/centre, filtered-message interactions and panic idempotency. Specifically prove filtering a CC does not discard a valid Note Off in the same block.
- Transport: offline simulation across arbitrary block sizes 32/64/128/512/1024, 44.1/48/96 kHz, tempo changes, discontinuities and clock ownership. Check timestamps are ordered and within block bounds and fractional timing does not accumulate drift.
- Real-time robustness: allocation/lock instrumentation after prepare, queue saturation, cancellation, concurrent UI events, stress bursts and release priority under overload. Use sanitizers on core/non-real-time code.
- Persistence: malformed/future schema, absent device, restored channel, no automatic note replay, old-version migration and multiple plug-in instances.
- macOS/hardware: arm64 standalone, Live 12 Lite VST3 scan/route/save/reopen, AU validation and chosen AU host, input monitoring feedback, hotplug, sleep/wake, permission denial and panic. Test 44.1/48 kHz and at least 64/256/1024 buffers first.
- Hardware regression: attach firmware version, port path, channel, HARMONY/KEY IN settings, monitor capture and audio recording. Log uncertainty where the manual lists a message but does not specify its musical response.

## Delivery order and current limitations

M1 routing proof → M2 safe note engine → M3 standalone → M4 clock → M5 host/AU polish → M6 release. Schedule estimates should follow M1 because MIDI wrapper behaviour, hardware availability and AU architecture are the primary unknowns.

The current code implements the shared MIDI engine and editor described in Implementation progress above. Standalone device management and chord pads have an initial implementation. External clock following, timing measurements, AU output backend, audio return and host/hardware acceptance tests remain outstanding. Read-only CoreMIDI discovery and incoming E-4 clock capture have been performed. No MIDI output messages were sent and no plug-in was installed into system or user plug-in folders. Earlier phase-0 observations below describe the original scaffold baseline, not the current dependency setup.

## Project placement

The accessible project listing contains local Codex project “E-4 VST/Standalone” at this folder. It does not contain a ChatGPT project named “E-4 VST/Stadalone” (or the corrected spelling). Available project-management tools can move chats but cannot upload PLAN.md into a ChatGPT project's files. This file is therefore the canonical project-local plan. Upload it manually if that separate ChatGPT project is desired; no unrelated chat has been moved.

## Primary references (checked 24 October 2026)

- Roland v1.02 MIDI chart: https://static.roland.com/manuals/E-4_manual_v102/eng/31849395.html
- Vocoder external notes, KEY IN and four voices: https://static.roland.com/manuals/E-4_manual_v102/eng/26654522.html
- Hardware menu, MIDI settings and sync: https://static.roland.com/manuals/E-4_manual_v102/eng/26654566.html
- Ableton VST MIDI routing and AU limitation: https://help.ableton.com/hc/en-us/articles/209070189-Accessing-the-MIDI-output-of-a-VST-plug-in
- JUCE CMake API: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
- JUCE baseline release: https://github.com/juce-framework/JUCE/releases/tag/8.0.10
