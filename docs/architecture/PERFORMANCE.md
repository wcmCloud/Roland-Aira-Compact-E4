# Shared performance contract

## Implemented foundation

`PerformanceSettings` is a fixed-size, trivially copyable value snapshot. Both
front ends obtain factory pad intervals from this model through `ChordPads`.
It holds four stable pad slots, printable ASCII names up to 24 characters,
one to four strictly increasing offsets (-24..24), inversion and octave values.
ChordPads applies inversion by repeatedly moving the lowest voice above the
highest, then octave placement. It rejects the whole chord if any output pitch
is outside MIDI range. The shared bank editor stages changes until Apply.

`PerformancePreset` defines a canonical 133-byte payload: E4PF magic, schema 1,
then four 32-byte slots (25 name bytes including zero terminator/padding, count,
inversion, octave biased by 2, and four offsets biased by 24). It does not dump
C++ structs. Decode validates the entire candidate before assigning it; wrong
size, magic, future version, malformed names, duplicate/unordered offsets and
invalid numeric values fail without changing the destination. Encode also
leaves its destination intact on failure. This is a new app preset payload,
embedded as Base64 in schema-6 plug-in state and exported as Base64 text in
.e4bank files. Schemas 1–3 migrate to factory pads; schemas 1–4 default scale processing to Off. Inversion/octave automation
parameters override corresponding embedded values at runtime. The payload does
not contain held notes or connection state. Standalone persists the active bank
on normal exit; loading a file only stages edits until Apply.

Source IDs 0..5 retain their existing meanings: external/host input, screen
keyboard, then four pads. IDs 6 and 7 reserve independent arpeggiator and
sequencer ownership. All sources and input channels share the same four-voice
allocator. A source release clears accepted and rejected holds, but cannot
release another source's matching pitch. Panic clears all owners. Editor close
continues to clear only the screen keyboard and pads; future generators must
apply their explicit latch/host ownership policy rather than expanding that
loop indiscriminately.

## Processing and publication rules

1. Capture raw input for visual feedback, then consume controller mappings.
2. Transform note input by the selected scale mode while preserving the mapping
   from each original hold to its output. Pitch bend bypasses note transforms.
3. Select direct, arp or sequence for the generated lane; manual sources remain
   separate. Use stable owners for every generated press and release.
4. Apply global transpose, output channel and the shared four-voice allocator.
5. Deliver through host MIDI output or the standalone sender.

UI/file validation builds a complete settings candidate outside the audio
callback. SettingsExchange serializes non-real-time writers with a mutex and publishes
canonical bytes through lock-free atomics. Its audio reader checks the revision
once before and after copying, returning immediately on an in-progress update;
it never retries or locks. The consumer owns its active snapshot. Never share
a mutable settings object between threads. Apply a replacement at
an explicit block/step boundary after releasing the affected old notes, discard
stale queued pad presses (preserving screen-key releases), and wait for fresh input. A rejected update preserves the
active snapshot. No file access, locks, device calls or dynamic allocation may
be introduced on the processing path. The current engine already snapshots pad
notes for release; future inversion/scale changes must retain that property.

## Validation

Core tests cover canonical preset round-trip, every truncated length, invalid
versions/fields, failed-import preservation, duplicate offsets, all source IDs,
invalid owners, direct/generated overlap and shared voice-limit recovery.
Existing chord, processor and actual VST3 wrapper regressions exercise the
factory settings through both app paths. These software checks do not establish
hardware timing or Ableton acceptance.

P1 tests additionally exercise every factory inversion across all 128 roots,
negative offsets, wide voicings, whole-chord range rejection, invalid replacement
preservation, 5,000 concurrent bank publications, actual text-file round-trip,
embedded host state, schema migration, automation and overlapping-owner cleanup.
Native component previews include the bank editor. OS file-picker interaction
and actual Live/hardware playback remain separate manual acceptance tasks.

## Scale transform (P2)

ScaleSettings is a bounded value with mode, tonic, type and pad inclusion.
MidiEngine maps original input pitches before transpose, retaining ownership
under the original source/channel/note index and reference-counting the mapped
output. Releases and releaseSource use the same deterministic mapping. setScale
panics before changing it, so no accepted hold can acquire a different mapping.
Reject-mode notes never gain ownership; fifth-voice rejected holds retain the
existing original-input rejection accounting. Quantisation collisions can share
a physical voice without sharing release ownership.

Direct input and screen keys are included; pads are opt-in. Reserved generator
sources currently bypass this stage. P3 must explicitly transform the arp input
pool, and P4 must define sequence inclusion, rather than double-transforming
generated notes. Pitch bend bypasses note mapping. Nearest searches bounded
MIDI pitches with a lower-note tie break. A distinct outsideScale rejection
reason feeds UI feedback. Scale changes clear queued UI performance and pad
snapshots along with engine ownership; no held-note replay is attempted.

Scale parameters are host-automatable and stored in schema 5, separate from
chord-only bank files. Standalone restores them with devices disconnected.
Raw input highlights remain before scale/transpose. Chord previews use an
explicit scale snapshot captured on opening, before global transpose.

## Controller mapping stage (P5)

ControllerProfile has eight fixed bindings and a strict 61-byte E4MP/version-1
wire format. Validation rejects unsupported values, duplicate action targets
and overlapping channel/omni input assignments. ValueExchange now generalizes
the chord-bank snapshot publication to controller profiles; writers remain
non-real-time and audio consumers perform one bounded atomic read attempt.
The wire payload is Base64 in .e4map files and schema-6 host state. Older host
state migrates to an all-disabled profile.

ControllerRouter consumes raw notes/CCs before scale/transpose. It tracks
original note holds by channel/note, including repeated counts and consumed
release policy. Profile replacement releases actions and retires their routes
without forgetting which later releases must still be consumed. A captured
note never triggers an action and must drain before a fresh press can act.
CC learning suppresses the captured value; momentary actions also require
release before rearming. High/low thresholds (80/48) provide hysteresis.
Continuous controllers map absolute values; repeated values after cleanup are
blocked until changed. Ownership saturation performs fail-closed Panic.

Screen pads keep sources 2–5; arp/sequence remain reserved 6/7; controller pads
use sources 8–11. Separate ChordPads instances preserve note snapshots and
independent screen/controller releases. Both pad ranges honour scale inclusion.
All sources share the same physical four-voice allocator. A controller Panic
is handled after router dispatch, avoiding recursive router mutation from the
action callback. Standalone parameter changes occur on its message-thread
drain; plug-in mappings update audio-owned overrides only.

MappedParameters holds transpose plus four inversion overrides. At block entry,
a changed host base value replaces the override and wins that block. Otherwise
incoming controller values can update it at MIDI sample offsets. Atomic
feedback is read by UI/state serialization; no host API is called from mapping
actions in the callback. This does not claim sample-accurate host automation
or recognition of a repeated unchanged host value as a new automation event.

Known limits: eight bindings, one input binding per target, notes restricted to
pad/Panic actions, no toggle/latch, and no generator mappings. Controller profiles
and chord banks export separately. Physical CC delivery depends on host routing
and control-surface reservations; JUCE software-host CC delivery is tested,
while physical Live acceptance remains outstanding.
