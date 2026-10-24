# Shared performance contract

## Implemented foundation

`PerformanceSettings` is a fixed-size, trivially copyable value snapshot. Both
front ends obtain factory pad intervals from this model through `ChordPads`.
It holds four stable pad slots, printable ASCII names up to 24 characters,
one to four strictly increasing offsets (-24..24), inversion and octave values.
Inversion/octave are stored for the chord-bank milestone; the current factory
player still uses its existing root-position voicings. No editable-bank UI or
preset file chooser is shipped yet.

`PerformancePreset` defines a canonical 133-byte payload: E4PF magic, schema 1,
then four 32-byte slots (25 name bytes including zero terminator/padding, count,
inversion, octave biased by 2, and four offsets biased by 24). It does not dump
C++ structs. Decode validates the entire candidate before assigning it; wrong
size, magic, future version, malformed names, duplicate/unordered offsets and
invalid numeric values fail without changing the destination. Encode also
leaves its destination intact on failure. This is a new app preset payload,
separate from the existing schema-3 plug-in state. Old sessions are unaffected.
File I/O, import/export controls, embedding and migration to a later plug-in
schema belong to P1. The payload does not contain held notes or connection state.

Source IDs 0..5 retain their existing meanings: external/host input, screen
keyboard, then four pads. IDs 6 and 7 reserve independent arpeggiator and
sequencer ownership. All sources and input channels share the same four-voice
allocator. A source release clears accepted and rejected holds, but cannot
release another source's matching pitch. Panic clears all owners. Editor close
continues to clear only the screen keyboard and pads; future generators must
apply their explicit latch/host ownership policy rather than expanding that
loop indiscriminately.

## Processing and publication rules for the next increments

1. Capture raw input for visual feedback, then consume controller mappings.
2. Transform note input by the selected scale mode while preserving the mapping
   from each original hold to its output. Pitch bend bypasses note transforms.
3. Select direct, arp or sequence for the generated lane; manual sources remain
   separate. Use stable owners for every generated press and release.
4. Apply global transpose, output channel and the shared four-voice allocator.
5. Deliver through host MIDI output or the standalone sender.

UI/file validation builds a complete settings candidate outside the audio
callback. P1 must add bounded publication and consumer-owned active snapshots;
never share a mutable settings object between threads. Apply a replacement at
an explicit block/step boundary after releasing the affected old notes, discard
stale queued presses, and wait for fresh input. A rejected update preserves the
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
