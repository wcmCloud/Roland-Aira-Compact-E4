# E-4 Companion in Ableton Live

Open `E4 Companion Routing Project/E4 Companion Routing.als` after installing the
VST3. The set was created in Live 12.4.7 Lite on 24 October 2026. Older versions
may not open this file; the routing recipe below is the portable reference.

## Saved MIDI route

1. E4 Controller: E-4 Companion VST3, MIDI From All Ins, Monitor In. Match the
   keyboard input more narrowly if other controllers send unrelated traffic.
2. E4 Send: MIDI From E4 Controller; **lower chooser E-4 Companion** (not Pre FX
   or Post FX); Monitor In; MIDI To E-4 (MIDI IN), channel 3.
3. E4 Audio Return: external input placeholder, Monitor Off. Select the actual
   E-4/interface audio input before enabling monitoring or recording. Audio
   device selection is a Live preference, not a portable setting in the set.

The controller plug-in generates silent audio. Audio effects such as reverb
belong on E4 Audio Return. The fourth blank audio track and stock return tracks
were preserved. No microphone recording is included. Hardware clock Sync and
Remote output were left off; only Track output was enabled for E-4 (MIDI IN).
Keep standalone output disconnected when using Live as the performance route.

## MIDI learn

Click MIDI learn in the plug-in or standalone. Choose a row's Learn button,
move a controller or press a note, choose the action, then Apply mappings.
Capture alone does not trigger an action or change the active profile. Release
note/button triggers and use a fresh gesture after Apply. Continuous controls
must move to a different value from the captured value. Load/Save profile uses
.e4map files and stages imports for review. Slots default Off.

Notes can trigger momentary pads or Panic. CCs can trigger those actions or
control transpose, pad inversions and pitch bend. A control/target can have only
one active binding; overlapping omni/channel bindings are rejected. Consumed
notes do not also play through; unchecked Consume permits the original note.
Panic always suppresses its triggering event. CCs are interpreted locally and
never sent as hardware parameter messages. No latch/arp/sequence controls ship.

Live must deliver a controller's MIDI to the track for plug-in learn to see it.
Enable Track input for that keyboard. A control assigned to a Live control
surface or Remote mapping may be consumed by the host. The JUCE VST3 software
host delivers CC20 successfully in the automated fixture; physical-controller
CC delivery in Live remains to be checked. If a CC does not reach the plug-in,
use Live's own MIDI Map mode on an exposed host parameter instead. Do not use
that workaround for pad triggers: they are MIDI-learn actions, not exposed
host parameters in this version.

## What has been checked

- E-4 MIDI IN/OUT endpoints discovered on the Mac; VST3 installed in the user's
  VST3 folder, scanned and loaded successfully in Live.
- Generated-MIDI output appeared in the send-track chooser. Live was restarted with the final tested bundle and the saved route reopened successfully. Saved set contents
  were inspected to confirm the plug-in output and channel-3 hardware route.
- A short computer-keyboard note was sent/released via the route. Live's MIDI
  input/output indicators showed recent traffic. This is software routing
  evidence, not confirmation of a hardware voice or an audible recording.
- Opening E-4 USB audio at 44.1 kHz repeatedly produced “Selected audio device
  is not available.” A briefly selected device was not counted as a pass. Live
  was restored to its working Mac microphone/system output configuration; the
  audio return remains unverified and Monitor Off.

- On 24 October 2026, the user confirmed four-note playback while listening
  through speakers connected directly to the E-4. No audio recording was captured;
  the MIDI indicator blink was not separately confirmed.

Pending: bend verification and chord/bend recording, physical-controller learn/CC delivery,
stop/Panic cleanup, Live save/reopen audible playback and USB audio/device diagnosis.
AU is still experimental for MIDI output and was not installed or advertised
as a working Live send route.

For the remaining chord test, drag test-chord.mid onto E4 Controller and play it while speaking into the E-4 mic. It contains C4/E4/G4/B4 (MIDI 60/64/67/71), one second held plus one second rest at 120 BPM. Check global transpose/scale settings first; stop the clip after the test. This file is provided for manual verification and was not automatically played.

For the USB audio issue, the manual requires AIRA LINK to be Off for a direct computer USB connection. Verify that setting on the E-4 before the next audio test; the current setting and firmware version have not been read remotely. Do not assume the selected clock/audio device has opened successfully until Live is stable and the return has signal.
