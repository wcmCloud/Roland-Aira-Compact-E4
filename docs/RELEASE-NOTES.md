E-4 Companion initial studio preview: standalone MIDI companion and VST3 for macOS, Windows and Linux; AU for macOS.

Use the signed, notarized macOS DMG when available. macOS ZIP files are unsigned CI development builds. Windows packages are unsigned portable previews. Linux packages require the native ALSA/JACK, X11 and font libraries. Select the architecture matching your plug-in host.

Windows: extract the ZIP; Standalone contains the app, VST3 contains the plug-in. The optional install.ps1 installs to the current user's folders and preserves the previous plug-in. Enable the user VST3 location in your DAW if needed. If PowerShell blocks the helper, open PowerShell in the extracted folder and run: powershell -NoProfile -ExecutionPolicy Bypass -File .\install.ps1. You can also copy the VST3 bundle manually to your DAW’s VST3 folder.
Linux: extract the archive and run ./install.sh; standalone goes to ~/.local/opt/e4-companion and VST3 to ~/.vst3, preserving an existing plug-in.

Native CI runs the engine, MIDI mapping, output, processor and VST3 wrapper tests. This does not replace physical E-4 and host acceptance on each platform. Four-note hardware playback was confirmed on macOS. AU MIDI output in Ableton remains experimental; use VST3 routing. MIDI clock, MIDI learn, cleanup and audio routing still need the documented studio checks.

No E-4 remote CC, SysEx, program changes, reverb, pitch or formant control is claimed. These are not exposed by E-4 v1.02 MIDI. Audio comes from the hardware.

Project code is dual MIT/GPL-3.0-only; JUCE and other dependencies retain their own licences. Packages include notices and corresponding source, including pinned JUCE.
