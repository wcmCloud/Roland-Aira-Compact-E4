# E-4 hardware and Ableton acceptance record

Status: outgoing/performance acceptance NOT RUN. Read-only port discovery and a five-second incoming E-4 clock capture passed; see [validation evidence](validation/README.md). Automated builds do not establish hardware performance.

Record before testing: date, E-4 firmware, macOS and Live versions, USB/DIN route, MIDI endpoint names, hardware channel, KEY IN/HARMONY state, audio interface and buffer size. Attach a MIDI monitor capture and short audio recording to each result. Use a modest listening level.

## Standalone

1. Launch the standalone app. Confirm it starts disconnected, including after a previous successful session. Select the E-4 output and optional controller input, then Connect. Verify the hardware channel matches.
2. With VOCODER, KEY IN and HARMONY enabled, play a single note and release it. Hold four different notes, then press a fifth: the first four must remain; the fifth should be rejected.
3. Hold a matching pitch on the external controller and the on-screen keyboard. Release the screen key first: the external note should remain. Release the controller: it should stop.
4. Hold each chord pad and release it. Verify Major, Minor, Sus2 and Maj7 intervals, up to four voices. Change root while holding a pad: the original chord should release without a replacement chord until a fresh press.
5. Set transpose +12, then play MIDI note 60: monitor output should show 72. Set −12: a fresh press should send 48. With a transposed chord held, change transpose: Note Offs must name the old output pitches, followed by bend centre; no chord should automatically restart.
6. Test boundary notes with an external keyboard/clip: 127 with +24 and 0 with −24 must not wrap or become clamped to a shared note. A subsequent valid note must still work.
7. Sweep bend to both extremes and release the slider. Verify centre returns; record the E-4's audible bend range rather than assume a semitone range.
8. Hold notes, switch to a different output and reconnect. Old endpoint should receive cleanup; new endpoint must not receive held-note replay. Disconnect, Panic and normal quit should also clean up notes.
9. Unplug USB while holding notes. Confirm the app reports removal within roughly a second and does not auto-reconnect. Replug, Refresh, Connect and verify no old notes replay. A physically missing endpoint cannot receive cleanup: record actual device behaviour.
10. Sleep/wake or pause the app longer than two seconds. Confirm explicit reconnect is required. Restart the app: channel/root/transpose and desired port choices should persist, while connection and held notes should not.

## Ableton VST3

1. Load the built VST3 on a MIDI controller track. Create a second MIDI track with MIDI From set to the controller track and the plug-in in the lower chooser; Monitor In. Set MIDI To to the E-4 endpoint and final hardware channel. Add a separate audio return from the E-4.
2. Verify clip notes, on-screen keyboard, pads and bend reach the device. UI gestures are sent at the next processing block; clip event sample offsets are retained inside the plug-in. Confirm Live's final output channel because internal routing can merge channels.
3. Hold a clip note, then hold the same pitch on the UI keyboard. Close the plug-in editor: the clip-owned note must continue. Its later clip Note Off must release it. A UI-only note must release when the editor closes, provided the host continues processing callbacks.
4. Automate channel, chord root and transpose separately while notes are held. Verify old notes release, fresh notes use new settings and no held notes replay. Repeat with 64/256/1024-sample buffers.
5. Test transport stop, bypass/unbypass, loop wrap and instance removal. Record any suspended-callback limits; an instance that receives no final processing callback cannot guarantee cleanup. Keep a recovery Panic path available.
6. Save/reopen a Live set. Settings should restore without emitting notes. Test a schema-1 or schema-2 development state if available; transpose should default to zero.
7. When testing sync, enable Live's MIDI output Sync as the sole clock master. Keep standalone internal clock disabled; incoming clock is not forwarded and plug-in sync stays with Live. Verify no duplicated clock path. Record hardware looper start/stop and manual arming requirements separately.

## Standalone internal clock (not yet measured)

1. Disable Live Sync and all other clock senders. Connect the output, select the matching USB/MIDI sync source on the E-4, enable internal clock, set 120 BPM and press Start. Capture Start followed by 24 clocks per quarter note. Record any manual looper arming required; do not assume Start records audio.
2. Capture ten minutes each at 60, 120 and 180 BPM under idle and CPU/UI load. Report pulse count, mean interval, worst error, jitter distribution and drift against an independent reference. Software simulation is not evidence of physical timing accuracy.
3. Change BPM while running. Check rate/phase transitions, no duplicate Start and no unexpected transport messages. Test 20 and 300 BPM at the bounds.
4. Press Stop twice: expect one Stop transition and no later pulses until fresh Start. Verify held notes release. Panic should stop transport too. Stop/Start restarts from a new origin; no Continue or song-position chase is supported.
5. Disconnect/switch route while running. Old port should receive Stop before it closes; new port must remain stopped. Quit/relaunch and verify clock master remains disabled while BPM persists.
6. Force a timing stall or sleep/wake. Expect transport to stop rather than emit a burst of stale pulses; explicit reconnect, enable and restart are required after a reported fault. A UI stall longer than two seconds should trigger the timing-thread heartbeat stop independently.
7. Confirm incoming clock/Start/Stop are not echoed. Verify internal clock is never enabled in a routing template whose master is Live.

## AU and release gates

The AU builds but does not yet have a supported direct MIDI-output path in Live. Do not mark AU performance acceptance complete merely because scanning succeeds. AU validation, the Steinberg VST3 validator, native UI inspection, clock timing measurements, signing/notarisation and cross-app endpoint ownership remain separate release gates.

## Result log

| Check | Environment / route | Result | Capture / notes |
| --- | --- | --- | --- |
| Standalone basic notes/chords | Pending | NOT RUN | |
| Standalone cleanup/hotplug | Pending | NOT RUN | |
| Transpose / note boundaries | Pending | NOT RUN | |
| Live VST3 routing/performance | Pending | NOT RUN | |
| Live editor-close / transport | Pending | NOT RUN | |
| State persistence/migration | Pending | NOT RUN |
| Standalone clock timing/transport | Pending | NOT RUN | |

Software-only progress: JUCE VST3 discovery/load and the 24 wrapper checks passed. See [wrapper results](validation/vst3-wrapper-2026-10-24.json). The hardware result rows above remain unchanged.

## Sender regression checks (physical results still pending)

- Generate a dense input burst while moving controls: the interface should stay responsive during normal sends, with note order preserved. Record actual latency; the input queue still drains on a UI timer.
- Press Panic with unsent notes queued: pending presses should be cancelled, clock should stop and actual sent notes should release. Then play a fresh note without reconnecting.
- Disconnect/quit during a burst: old endpoint should receive cleanup, and queued presses must not migrate to a new route.
- On sender overflow, timing failure or a UI stall over two seconds, verify cleanup and explicit reconnection. A permanently blocked driver send may delay shutdown; note any driver-specific limits.

## Editable chord-bank acceptance

1. In standalone, open Edit / load chord bank; choose a template, rename it, select an inversion and octave, and check the preview against the MIDI monitor before global transpose. Apply and play the pad with E-4 HARMONY enabled.
2. Hold a matching external keyboard note while replacing a pad bank. Pad-only notes must release and the keyboard-owned note must continue. A replacement must not automatically sound.
3. Save a .e4bank file, change edits, load it, confirm loading alone leaves active pads unchanged, then Apply. Check cancel and invalid-file preservation. Close/reopen standalone: the applied bank should return with ports disconnected.
4. In Live, save a project with an edited bank, remove access to the exported file, then reopen. Verify names, voicings and no automatic note replay. Automate Pad 1 inversion/octave while held; verify old notes release and a fresh press uses the new voicing.
5. Confirm an out-of-range chord produces no partial voicing. Test overlapping pads, root/transpose changes, Panic, editor close, bypass and disconnect. Verify old projects restore factory pads.

## Key/scale acceptance

1. Choose C / Major / Nearest; play C4 and C#4 together. Monitor one C4 output; releasing either first must preserve the other, including repeated holds and different input channels. Raw highlights should remain on the input keys.
2. Choose Reject; C#4 should produce no Note On, while C4 still plays. Test pitch bend separately: it must still work. Include pads, play Minor, and confirm the minor third is filtered; disable pad inclusion to restore the full pad.
3. Change key/mode/type/pad inclusion with held notes; all old output notes must release without replay. Check Panic, editor close, bypass, port changes and reconnect for stranded notes.
4. Check boundary notes 0/127, nearest ties, and scale then transpose ordering. Save/reopen standalone and a Live project; verify scale recall. Open an older project: scale processing must default Off.
5. In Live, automate scale mode/key/type/inclusion and record MIDI plus E-4 audio. Confirm timing and release behaviour. Reopen the chord-bank editor after scale changes to refresh its preview snapshot.

## Controller mapping acceptance

1. Open MIDI learn, select an Off row, press a note or move a CC. Capture must not sound notes or change the current action. Choose an action, Apply, release the trigger and use a fresh gesture. Cancel and malformed/conflicting imports must preserve the active profile.
2. Hold a mapped pad and its matching screen pad. Releasing either must preserve the other. Include matching direct input and overlapping MIDI channels/repeated holds. Edit the profile/bank while held; old releases must not cut new owners or fire replacement actions.
3. Test CC button values 0/79/80/127/64/48/0; repeated highs must not retrigger and release must occur at 48. Panic should release mapped notes, centre bend and require a fresh gesture.
4. Test transpose CC endpoints (-24/+24), each inversion, bend endpoints/centre/deadband and profile save/load. In Live, change a host parameter in the same block as a controller event: changed host values should win. No CC may reach the E-4 output.
5. Save/reopen the host set without external profile files, restart standalone, and open an older project. New settings must persist without replay; old projects must have mappings Off.
6. Verify a real controller with Track input enabled in Live. If a control is reserved by a control-surface script, document it and test the host MIDI Map alternative for numeric parameters. Verify disconnect, stop/bypass, editor close, sleep/wake and overload.

Current Live routing and audio-device observations are recorded in docs/ableton/README.md. A selected device or moving software MIDI indicator does not prove audible E-4 operation.
