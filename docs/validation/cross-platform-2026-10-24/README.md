# Cross-platform release preparation — 24 October 2026

Universal macOS Release binaries built from JUCE 8.0.10: standalone, VST3, AU, graphical installer and MIDI probe all contain arm64 and x86_64. The VST3 passed the official Steinberg validator (47 passed, 0 failed) on Apple Silicon. The local Debug suite passed all eight CTest cases.

The universal DMG was Developer ID signed, Apple-notarised and stapled; Gatekeeper accepted it. The separate unsigned PKG is not a release installer. Intel execution was not tested locally because Rosetta is not installed; native CI covers Intel compilation and automated tests. Hardware and real DAW acceptance remain pending outside the existing macOS checks.

All six native builds passed their full eight-case CTest suites and packaged successfully (macOS arm64/x64, Windows arm64/x64, Linux arm64/x64). Initial native CI: https://github.com/wcmCloud/Roland-Aira-Compact-E4/actions/runs/40122166652

The portable macOS archive was inspected for standalone, AU, licence and JUCE source contents and its SHA-256 verified. Windows/Linux installation and hardware checks require their native systems.
