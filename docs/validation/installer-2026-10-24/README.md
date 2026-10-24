# macOS installer verification — 24 October 2026

Release 0.1.0, native arm64. Artifacts remain in ignored build-packages/0.1.0-arm64.

- Release VST3: official Steinberg validator reports 47 passed, zero failed.
- Application, AU and VST3 payloads: Developer ID Application signed with secure timestamps/hardened runtime; strict signatures verified.
- Final DMG: Apple notarization Accepted (see dmg-notary.json), ticket stapled and validated, Gatekeeper assessment accepted with source Notarized Developer ID.
- Mounted final DMG read-only and executed its Install.command with an isolated HOME: all three bundles installed successfully. No actual user/system plug-in directories changed by this test.
- Separate isolated fresh-install/reinstall test: all three previous bundles backed up, installed signatures valid.
- PKG: expanded successfully; extracted VST3 signature verified; macOS installer -showChoicesXML accepts all three selectable components. Earlier expanded AU/standalone payloads were not independently host-tested. PKG system installation not executed.
- PKG is explicitly UNSIGNED; this Mac has no Developer ID Installer private-key identity. It has not been notarized. The packaging script signs/notarizes/staples it when INSTALLER_IDENTITY is provided. The Apple Distribution identity is not a substitute.
- Checksums of final DMG and unsigned PKG are recorded in SHA256SUMS.txt.

This is not a clean macOS account/machine certification. Intel, Windows, Linux and macOS 11 runtime compatibility remain untested. AU format validation of the earlier development build does not establish Live MIDI-output support. Existing hardware verification limits remain unchanged.
