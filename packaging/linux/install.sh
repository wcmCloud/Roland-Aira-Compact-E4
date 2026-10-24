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
cp e4-companion.png "$appdir/"
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/e4-companion.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=E-4 Companion
Comment=Roland AIRA E-4 MIDI companion
Exec="$appdir/Standalone/E-4 Companion"
Icon=$appdir/e4-companion.png
Terminal=false
Categories=AudioVideo;Audio;Midi;
EOF
