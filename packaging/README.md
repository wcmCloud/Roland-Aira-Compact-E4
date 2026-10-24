# Installers and platform support

The JUCE C++ sources can target macOS, Windows and Linux. Binaries and installers are platform-specific: AU is Apple-only; VST3 needs native builds for each OS and CPU architecture. Only macOS arm64 has currently been built and tested. Windows/Linux support is planned, not certified.

## macOS

Build Release:

```sh
cmake -S . -B build-installer -DCMAKE_BUILD_TYPE=Release -DJUCE_SOURCE_DIR="$PWD/build/_deps/juce-src" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DBUILD_TESTING=OFF
cmake --build build-installer --target AiraE4_VST3 AiraE4_AU AiraE4_Standalone -j 6
python3 scripts/package-macos.py --signing-config /path/to/signing.local.env --notarize
```

Configuration is parsed as data, not executed. It accepts these names (environment variables take precedence):

```sh
SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
INSTALLER_IDENTITY="Developer ID Installer: Your Name (TEAMID)"
NOTARY_PROFILE="your-existing-keychain-profile"
```

Keep passwords and signing keys in Keychain. Do not commit local signing configuration. This Mac reuses the Application identity and `maXfolders` notarytool profile referenced by the T-8 project's `scripts/signing.local.env`; it currently has no Developer ID Installer identity. Apple Distribution is a different certificate type and cannot replace it.

The packaging tool signs staged copies with hardened runtime and secure timestamps, verifies signatures, creates a choice-based .pkg and a .dmg, submits signed deliverables to Apple, requires Accepted, staples tickets, validates them, runs Gatekeeper assessment and writes SHA256SUMS. Sources/build outputs are not modified. Artifacts and detailed logs stay under ignored build-packages. Earlier output directories are preserved on repeat runs.

- **DMG:** signed/notarized with the Application certificate. Open it and run Install.command to install all three formats into your user folders. It backs up existing copies and rolls back failures. No admin password is required. Alternatively copy only wanted formats manually. Quit hosts before installing and restart/rescan afterwards.
- **PKG:** offers VST3, experimental AU and standalone choices; installs in `/Library/Audio/Plug-Ins/VST3`, `/Library/Audio/Plug-Ins/Components` and `/Applications`. It requests administrator authorization, uses fixed destinations and upgrades bundles. It does not back up overwritten system bundles or delete user copies. When no Installer identity is supplied, its filename explicitly says UNSIGNED and it is not submitted for notarization. Do not distribute that unsigned package as a finished release.

For the signed PKG path, create/import a **Developer ID Installer** certificate with its private key through the Apple developer account, add its identity to the local configuration and rerun the same command. PKG signing, notarization, stapling and assessment then happen automatically. Do not substitute an Application or Apple Distribution identity.

User VST3 copies take priority over global copies. Choose one installation scope; move duplicate E-4 bundles aside manually before switching. No installer deletes preferences, banks, mappings or DAW sets. To uninstall, quit hosts and move only installed E-4 bundles to Trash; restore backups if desired. Removing files does not remove macOS package receipts.

The current build is arm64 only and declares macOS 11 minimum; oldest-OS compatibility is not tested. Universal `arm64;x86_64` builds need separate build/validator/host acceptance. AU validation does not establish usable MIDI output in Live: use VST3 there. Signing/notarization does not replace clean-account and hardware acceptance.

## Windows and Linux next steps

CMake selects VST3 everywhere and AU only on Apple platforms. Build VST3 and the standalone natively with the platform JUCE dependencies. Then validate MIDI APIs, routing, timing, UI, state and cleanup on that OS before claiming support.

- Windows: produce a signed x64 installer (for example WiX/NSIS) targeting the standard Common Files/VST3 location plus the standalone application. macOS signing/notarization does not apply; Windows code signing is separate.
- Linux: produce an architecture-specific archive/package installing VST3 to `~/.vst3` or an appropriate system VST3 folder, plus the standalone binary. Validate distribution/library compatibility.

No Windows/Linux installer or binaries are included yet. See [Steinberg plug-in locations](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html) and [Apple notarization requirements](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).
