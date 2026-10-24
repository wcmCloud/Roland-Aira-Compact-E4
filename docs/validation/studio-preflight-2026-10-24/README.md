# Studio preflight — 24 October 2026

Native Apple Silicon Release preview 0.1.0 on this Mac; Live 12.4.7 Lite. No studio acceptance is implied.

## Passed locally

- Installed signed Release VST3; Live rescanned it and reopened the saved routing set without a missing-plug-in error.
- Host parameter exposure and editing: transpose changed from 0 to +2, saved into a separate E4 Studio Acceptance set, switched to the original routing set, then reopened the acceptance set. Live displayed restored +2. Neutral transpose restored afterwards.
- A temporary second controller track/plug-in instance loaded successfully and was removed afterwards. This is a load/remove smoke test, not shared-endpoint ownership certification.
- Device activation off/on and plug-in-window open/close toggles responded. The floating editor itself is not exposed reliably to the automation interface; visual/controller interaction and held-note cleanup still require studio checks.
- Transport start/stop responded with the Release instance loaded, with no visible error. No held-note cleanup claim is made from an idle transport test.
- A temporary receive-only CoreMIDI endpoint captured Live's generated output on channel 3. Three initial notes (60/64/67), then one note each at 64/256/1024 samples, produced six Note Ons and six matching Note Offs with no outstanding notes. Bend messages captured here were centre resets only. See live-output.json and live-output-analysis.json. Buffer restored to the observed original 512 samples, E-4 output/channel 3 restored, transport stopped, audio return Monitor Off.
- Apple auval succeeds on the installed Release AU; official Steinberg validator reports 47 passed, zero failed on Release VST3. The installed Release VST3 passes 31 project wrapper checks. Automated project results are in ctest.txt; any environment limitations are recorded separately below.

- Installed signed standalone launches disconnected with clock off; MIDI learn and chord-bank dialogs open and cancel normally. Existing user channel/root preferences were preserved; match channel 3 explicitly when connecting in the studio.

- Graphical installer engine: isolated fresh install, reinstall/backups, all three installed signatures, forced later-format failure/nonzero status and restoration of the old VST3 pass (gui-installer.json). Native installer layout inspected; final labels use ASCII punctuation to avoid encoding artifacts.
- New original app icon rendered at 16–1024 pixels and embedded in standalone, AU/VST3 resources and graphical installer; editable SVG and ICNS/ICO sources are retained in the repository.
- Studio controller profile decoded successfully with the actual ControllerPreset codec before packaging.

## Still for the studio

Audible bend extremes/centre/range; physical keyboard MIDI learn and CC delivery; custom bank/profile/scales plus host save/reopen; held-note stop/bypass/editor-close/instance-removal behavior; multiple-instance independence under load; clock/looper response and timing; hotplug/sleep; USB/analog audio return and recordings. Four-note E-4 playback was previously user confirmed. Fifth-note rejection remains a separate hardware check.

The DMG contains signed payloads and the test kit, and is submitted to Apple's notary service after kit assembly. Consult the final artifact's notary report and checksums. The system PKG remains unsigned until a Developer ID Installer certificate is available. Use the signed/notarized DMG for this handoff.

Only arm64 is built/tested. A real clean-account/machine installation and macOS 11 runtime compatibility remain unverified. Keep source wrapper results, Live MIDI output and physical E-4 audio results distinct.

## Final artifact verification

The final icon-bearing DMG is Accepted by Apple (dmg-notary.json), stapled and Gatekeeper accepted. All five shipped code bundles/tools pass strict signatures; Gatekeeper explicitly accepts both the standalone and graphical installer. Mounted final image installation passes. The graphical Install button then completed installation into the actual user folders; all files match the final signed payloads, with embedded icons verified. Both official validators and all 31 wrapper checks pass again on these installed bundles. Live rescanned the final VST3 and reopened E4 Studio Acceptance with transpose 0, hardware channel-3 route and stopped transport. The image includes the portable studio kit and signed diagnostics. SHA256SUMS.txt identifies the final artifacts. The unsigned development PKG is not the studio handoff; use the notarized DMG graphical installer.

## README and licensing refresh

The final DMG was rebuilt after the T-8-style README and MIT OR GPL-3.0-only project licensing were added. Its Studio Test Kit/Licences files match the repository texts. Studio Test Kit/Sources includes the build/project sources and JUCE 8.0.10, excluding the Roland manual, credentials, Git history and build outputs. Extracted sources configure successfully against included JUCE and pass core, clock, mapping and sender checks. The final image is Apple Accepted, stapled and Gatekeeper accepted; this folder’s notary report/checksums now identify this refreshed image. Linked binaries retain JUCE AGPL/commercial terms as documented in THIRD_PARTY_NOTICES.md.

## Visible branding and screenshot refresh

The E-4 logo is embedded as binary image data and painted inside the standalone/plug-in headers and graphical installer header, in addition to OS bundle icons. README adds native macOS standalone, installer, controller-settings and chord-settings window captures; the plug-in image remains an explicitly labelled native component preview. All eight project checks and Steinberg’s 47 checks pass after the UI change. Final packaging includes the updated sources and images.

The visible-branding build was installed through the graphical installer, and the new header logo was visually confirmed in the installed standalone. Installed AU validation and all 31 VST3 wrapper checks pass. Final payload file hashes match the installation; notarisation and checksums above identify the screenshot/logo refresh. Hardware acceptance remains unchanged.
