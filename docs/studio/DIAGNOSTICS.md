# Optional MIDI diagnostics

The signed Tools/e4_midi_probe is receive-only: it opens no MIDI output and changes no E-4 settings. It can list ports, capture an existing input, or create a temporary virtual MIDI destination for Live output checks. The command-line tool is optional; ordinary playing tests do not require it.

From Terminal, change to the copied kit's Tools folder, then:

```sh
./e4_midi_probe --list
./e4_midi_probe --capture "INPUT_IDENTIFIER_FROM_LIST" --seconds 10 > capture.json
./e4_midi_probe --virtual "E4 Studio Monitor" --seconds 60 > live-output.json
```

For the virtual mode, enable Track output for E4 Studio Monitor in Live's Tempo & MIDI settings and temporarily select it on E4 Send. Keep Sync/Remote off for a note test. Restore E-4/channel 3 before testing hardware. The temporary monitor endpoint disappears when the capture ends. Valid duration is 1–600 seconds; the tool waits until it ends and then writes JSON. Keep the Terminal open during capture.

The report counts notes, bend, clock and transport and stores up to 256 short messages. It does not record audio or long SysEx. `hardwareValidated` stays false by design: a receive capture cannot establish outgoing hardware performance. Clock statistics use local callback arrival times, including USB/OS batching; this is not a hardware timestamp guarantee or outgoing jitter certification. An E-4 input capture measures what the E-4 sends, not what the app sends to it. To measure app clock output, route it to an independent receiver/reference and document the path.
