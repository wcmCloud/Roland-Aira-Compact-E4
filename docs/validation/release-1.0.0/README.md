# Release 1.0.0 validation

Published: https://github.com/wcmCloud/Roland-Aira-Compact-E4/releases/tag/v1.0.0

Native CI: https://github.com/wcmCloud/Roland-Aira-Compact-E4/actions/runs/40125030694

All six native builds passed all eight CTest cases and packaging: macOS arm64/Intel, Windows x64/ARM64, Linux x64/ARM64. The local Debug suite also passed all eight cases. The universal macOS VST3 passed the official Steinberg validator: 47 passed, zero failed.

The standalone, graphical installer, AU and VST3 report version 1.0.0 and contain arm64 and x86_64 Mac executables. The universal DMG is Developer ID signed, Apple-notarised and stapled; stapler validation and Gatekeeper assessment passed. Gatekeeper also accepted the installer and standalone app. The unsigned PKG is excluded from the release.

All four Windows/Linux archives were inspected for the correct native executable architecture, UTF-8 installation instructions, installation helpers, licences and corresponding source. Linux archives include the icon and desktop-entry installer. The separate corresponding-source archive declares project version 1.0.0 and includes pinned JUCE sources.

SHA-256 checksums were verified for all six primary release archives. GitHub's server-side digests matched all twelve uploaded archive/checksum files; the combined SHA256SUMS.txt is the thirteenth release asset.

This regular release includes the compact 56-pixel header and taller keyboards. It does not claim additional physical E-4 or DAW certification: Windows binaries remain unsigned, Windows/Linux and Intel hardware/host installation acceptance remains pending, and AU MIDI output in Ableton remains experimental. Four-note hardware playback and the previously documented macOS routing checks remain the available physical/host evidence.
