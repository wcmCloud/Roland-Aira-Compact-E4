# Formal validation and development installation — 24 October 2026

Tested native arm64 Debug 0.1.0 bundles on the current Mac. This is a fresh bundle replacement on the existing user account, not a clean-account or distribution certification.

## Results

- Steinberg official VST3 validator: **47 passed, 0 failed** on the installed VST3. SDK commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`; built Release with GUI support and plug-in examples disabled. See `steinberg-vst3.txt`. Unsupported 64-bit audio processing is reported/skipped; this does not certify double-precision processing.
- Apple `auval -v aumu Aefr Chrs`: **AU VALIDATION SUCCEEDED**, exit 0, on the installed AU. See `auval.txt`. AU registration took a subsequent discovery pass after first installation.
- All eight CTest checks pass; see `ctest.txt`.
- Installed VST3 passes the project's 31 wrapper checks; see `installed-vst3.json`.
- Both installed bundles pass strict code-signature verification and SHA-256 comparison of every source file. Signatures are ad hoc development signatures, not Developer ID or notarisation. See `install.txt`; the backup directory it identifies contains the complete file hash manifest.

The first Steinberg run found one failure: the advertised factory program had an empty name. `getProgramName()` now returns Default; both bundles were rebuilt, freshly installed and validated again. The original AU development signature was invalid; the installer explicitly ad-hoc signs and verifies development bundles before installation.

## Repeat installation

Quit hosts before replacing bundles; an already loaded host can retain the old code until restarted. From the repository root:

```sh
cmake --build build-check --target AiraE4_VST3 AiraE4_AU -j 6
python3 scripts/install-dev-plugins.py --au
auval -v aumu Aefr Chrs
build-check/validation-tools/build/bin/Release/validator "$HOME/Library/Audio/Plug-Ins/VST3/E-4 Companion.vst3"
```

Omit `--au` to install only VST3. The script backs up existing bundles under the selected build directory, installs whole fresh bundles, checks signatures and hashes, and rolls back all replacements if installation fails. It does not remove other plug-ins or clear system caches. To restore, quit hosts, move the newly installed E-4 bundles aside, and restore their matching backup bundles to the user plug-in folders; an AU without an earlier backup can simply be moved aside. App preferences and Live sets are not removed.

Validator sources are downloaded separately from the [official Steinberg SDK](https://github.com/steinbergmedia/vst3sdk), kept in ignored build-check/validation-tools, and are not vendored into this project. Build instructions follow its official CMake workflow.

## Remaining release gates

- Clean macOS account/machine install, scan, uninstall and reinstall.
- Developer ID signing, notarisation, distributable installer, final identifiers/licensing and any Intel/universal support.
- Reload the corrected bundle in Live before further host acceptance; replacing files does not reload an already open instance.
- AU MIDI output in Live remains experimental/unsupported despite format validation passing.
- Hardware bend, controller mapping, cleanup/hotplug, clock measurements and USB audio return remain separate checks. Four-note playback was confirmed by the user before this validation.
