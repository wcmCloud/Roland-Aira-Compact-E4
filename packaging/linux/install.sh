#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
appdir="$HOME/.local/opt/e4-companion"
plugindir="$HOME/.vst3"
mkdir -p "$appdir" "$plugindir"
stamp=$(date +%Y%m%d-%H%M%S)
if [ -d "$plugindir/E-4 Companion.vst3" ]; then
    mv "$plugindir/E-4 Companion.vst3" "$plugindir/E-4 Companion.vst3.backup-$stamp"
fi
cp -R 'VST3/E-4 Companion.vst3' "$plugindir/"
cp -R Standalone Licences Sources Studio-test-kit "$appdir/"
chmod +x "$appdir/Standalone/E-4 Companion"
printf 'Installed. Launch: %s\n' "$appdir/Standalone/E-4 Companion"
